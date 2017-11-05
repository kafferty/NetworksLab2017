#undef UNICODE

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x501
#include <iostream>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include <vector>
#include <fstream>
#include <algorithm>
#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27021"

std::vector< std::pair<int, SOCKET> > poolOfSockets; //Вектор пар для клиентов
HANDLE mainThreadMutex;
std::vector< std::pair<SOCKET, std::string> > loggedInClients;

std::vector<std::string> split(std::string s, std::string delimiter){ //Разбивает строку по делиметру
    std::vector<std::string> list;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) { //пока не конец строки
        token = s.substr(0, pos);
        list.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    list.push_back(s);
    return list;
}

int sendn(int sendSocket, char *buffer, int n) { // Функция, посылающая >= n байт
    int bytesSent = 0;
    while(bytesSent < n) {
        int sendResult = send(sendSocket, buffer, DEFAULT_BUFLEN, 0);
        if (sendResult == SOCKET_ERROR) {
            return SOCKET_ERROR;
        }
        buffer = buffer + DEFAULT_BUFLEN;
        bytesSent = bytesSent + DEFAULT_BUFLEN;
    }
    return bytesSent;
}

int readn(int newsockfd, char *buffer, int n) {
        int nLeft = n;
        int k;
        while (nLeft > 0) {
            k = recv(newsockfd, buffer, nLeft, 0);
            if (k < 0) {
                perror("ERROR reading from socket");
                return -1;
            } else if (k == 0) break;
            buffer = buffer + k;
            nLeft = nLeft - k;
        }
        return n - nLeft;
}

unsigned int __stdcall threadedFunction(void* pArguments) {

        SOCKET ClientSocket = *(SOCKET*) pArguments;
        std::string endString = "end";
        std::string registerString = "register";
        std::string showTestsString = "show";
        std::string getTestResult = "getResult";
        std::string getTest = "getTest";
        int readed;
        do {
            char recvbuf[DEFAULT_BUFLEN];
            int recvbuflen = DEFAULT_BUFLEN;
            int iSendResult;
            int iResult;
            char *p = recvbuf;
            ZeroMemory(p, recvbuflen);

            readed = readn(ClientSocket, p, recvbuflen);
            if (readed == -1 || readed == 0) {
                shutdown(ClientSocket, 2);
                closesocket(ClientSocket);
                break;
            }
            printf("Bytes read: %d\n", readed);
            printf("Received data: %s\n", recvbuf);
            if (recvbuf == endString) { //8) Обработка запроса на отключение клиента
                shutdown(ClientSocket, 2);
                closesocket(ClientSocket);
                int deleteSock;
                WaitForSingleObject(mainThreadMutex, INFINITE);
                for(unsigned int i = 0; i < poolOfSockets.size(); i++) {
                    if (poolOfSockets[i].second == ClientSocket) {
                        deleteSock = i;
                        break;
                    }
                }
                poolOfSockets.erase(poolOfSockets.begin() + deleteSock);
                ReleaseMutex(mainThreadMutex);
                break;
            } else {
                if (std::string(recvbuf) == registerString) { //4) Регистрация клиента
                    std::vector<std::string> loginVector;
                    std::ifstream fromRegisterFile;
                    fromRegisterFile.open("registered.txt");
                    std::string currentString;
                    if (fromRegisterFile.is_open()) {
                        while(getline(fromRegisterFile, currentString)) {
                            std::vector<std::string> temp;
                            temp = split(currentString, " ");
                            loginVector.push_back(temp[0] + " " + temp[1]);
                        }
                        fromRegisterFile.close();
                    }

                    std::ofstream registerFile;
                    registerFile.open("registered.txt", std::ios::app);
                    while(true) {
                        readed = readn(ClientSocket, p, recvbuflen);
                        std::string infoString(recvbuf);
                        if(split(infoString, " ").size() == 2) {
                            bool flag = false;//для выхода из цикла
                            for(unsigned int i = 0; i < loginVector.size(); i++) {
                                if (loginVector[i] == infoString) {
                                    bool isOnline = false;//залогинин ли
                                    for(unsigned int i = 0; i < loggedInClients.size(); i++) {
                                        if (loggedInClients[i].second == infoString) {
                                            isOnline = true;
                                            break;
                                        }
                                    }
                                    if (!isOnline) {
                                        char successfulLogin[DEFAULT_BUFLEN] = "You are successfully logged in!";
                                        loggedInClients.push_back(std::make_pair(ClientSocket, infoString));
                                        sendn(ClientSocket, successfulLogin, strlen(successfulLogin));
                                        flag = true;
                                    } else {
                                        char alreadyOnline[DEFAULT_BUFLEN] = "This account is online!";
                                        sendn(ClientSocket, alreadyOnline, strlen(alreadyOnline));
                                    }
                                    break;
                                }
                            }
                            if (flag == true)
                                break;
                            else {
                                bool noLogin = false;
                                for(unsigned int i = 0; i < loginVector.size(); i++) {
                                    if (split(infoString, " ")[0] == split(loginVector[i], " ")[0]) {
                                        char badLogin[DEFAULT_BUFLEN] = "Try another password or login!";
                                        sendn(ClientSocket, badLogin, strlen(badLogin));
                                        noLogin = true;//есть логин
                                        break;
                                    }
                                }
                                if (noLogin == false) {//если нет такого логина
                                    bool reLogin = false;
                                    for(unsigned int i = 0; i < loggedInClients.size(); i++) {//если клиент пытается зайти за другого юзера
                                        if (loggedInClients[i].first == ClientSocket) {
                                            loggedInClients[i].second = infoString;
                                            reLogin = true;
                                            break;
                                        }
                                    }
                                    if (!reLogin) {
                                        loggedInClients.push_back(std::make_pair(ClientSocket, infoString));
                                    }
                                    infoString = infoString + " no_result\n";
                                    registerFile << infoString;
                                    registerFile.close();
                                    char successfulRegistration[DEFAULT_BUFLEN] = "You are successfully registered";
                                    sendn(ClientSocket, successfulRegistration, strlen(successfulRegistration));
                                    break;
                                }
                            }
                        } else {
                            printf("Wrong number of argumenst\n");
                            char wrongNumber[DEFAULT_BUFLEN] = "Wrong number of argumenst";
                            sendn(ClientSocket, wrongNumber, strlen(wrongNumber));
                        }
                    }
                } else {
                    if (recvbuf == showTestsString) { //4) Выдача клиенту списка тестов
                        char sendbuf[DEFAULT_BUFLEN] = "1 - Math test\n2 - Phsycological test\n";
                        sendn(ClientSocket, sendbuf, strlen(sendbuf));
                    } else {
                        if (recvbuf == getTestResult) { //4) Выдача клиенту результата его последнего теста
                            std::string currentClientInfo = "noInfo";
                            for(unsigned int i = 0; i < loggedInClients.size(); i++) {
                                if (loggedInClients[i].first == ClientSocket) {
                                    currentClientInfo = loggedInClients[i].second;
                                    break;
                                }
                            }
                            if (currentClientInfo != "noInfo") {//сюда не зайти, если не залогинен
                                std::ifstream fromRegisterFile;
                                fromRegisterFile.open("registered.txt");
                                std::string currentString;
                                if (fromRegisterFile.is_open()) {
                                    while(getline(fromRegisterFile, currentString)) {
                                        std::vector<std::string> temp;
                                        temp = split(currentString, " ");
                                        if (currentClientInfo == temp[0] + " " + temp[1]) {
                                            char yourResult[DEFAULT_BUFLEN] = "Your last result was: ";
                                            strcat(yourResult, temp[2].c_str());
                                            sendn(ClientSocket, yourResult, strlen(yourResult));
                                        }
                                    }
                                    fromRegisterFile.close();
                                }
                            } else {
                                char registerFirst[DEFAULT_BUFLEN] = "You have to register first";
                                sendn(ClientSocket, registerFirst, strlen(registerFirst));
                            }

                        } else { //5) Получение от клиента номера теста
                            std::vector<std::string> splitVect = split(std::string(recvbuf), " ");
                            if ((splitVect.size() > 1) && (splitVect[0] == getTest)) {
                                std::string currentClientInfo = "noInfo";
                                for(unsigned int i = 0; i < loggedInClients.size(); i++) {
                                    if (loggedInClients[i].first == ClientSocket) {
                                        currentClientInfo = loggedInClients[i].second;
                                        break;
                                    }
                                }
                            if (currentClientInfo != "noInfo") {
                                    std::string testString = "test" + splitVect[1] + ".txt";
                                    std::string ansTestString = "ans" + testString;
                                    std::ifstream file;
                                    std::string question;
                                    std::vector<std::string> vec;
                                    int summary = 0;
                                    file.open(testString.c_str());
                                    if (file.is_open()) {
                                        while(std::getline(file, question)) {
                                            char buf[DEFAULT_BUFLEN];
                                            for (unsigned int i = 0; i <= question.size(); i++) {
                                                buf[i] = question[i];
                                            }
                                                //6) Посылаем вопрос
                                            sendn(ClientSocket, buf, strlen(buf));
                                            if (std::getline(file, question)) {
                                                for (unsigned int i = 0; i <= question.size(); i++) {
                                                    buf[i] = question[i];
                                                }
                                                //6) Посылаем варианты ответов
                                                sendn(ClientSocket, buf, strlen(buf));
                                            }
                                            readn(ClientSocket, p, DEFAULT_BUFLEN); //6) Получение ответов на вопросы
                                            vec.push_back(std::string(recvbuf));
                                        }
                                        file.close();
                                        std::ifstream ansFile;
                                        ansFile.open((ansTestString.c_str()));
                                        std::string answer;
                                        if (ansFile.is_open()) {
                                            int counter = 0;
                                            while(getline(ansFile, answer)) { //Проверяем ответы клиента
                                                if (vec[counter] == answer) {
                                                    summary++;
                                                }
                                                counter++;
                                            }
                                        }
                                        ansFile.close();
                                        char conv[DEFAULT_BUFLEN];
                                        itoa(summary, conv, 10);
                                        char numberOfQuest[DEFAULT_BUFLEN];
                                        itoa(vec.size(), numberOfQuest, 10);
                                        char resultingString[DEFAULT_BUFLEN] = "Your result is ";
                                        std::string toConcat = std::string(conv) + " out of " + std::string(numberOfQuest);
                                        strcat(resultingString, toConcat.c_str());
                                        sendn(ClientSocket, resultingString, strlen(resultingString));
                                        std::string toRegisterFile = std::string(conv) + "_out_of_" + std::string(numberOfQuest);
                                        std::string currentClientInfo = "noInfo";
                                        for(unsigned int i = 0; i < loggedInClients.size(); i++) {
                                            if (loggedInClients[i].first == ClientSocket) {
                                                currentClientInfo = loggedInClients[i].second;
                                                break;
                                            }
                                        }
                                        std::vector<std::string> copyVector; //Запись последнего результата в файл
                                        if (currentClientInfo != "noInfo") {
                                            std::ifstream fromRegisterFile;
                                            fromRegisterFile.open("registered.txt");
                                            std::string currentString;
                                            if (fromRegisterFile.is_open()) {
                                                while(getline(fromRegisterFile, currentString)) {
                                                    std::vector<std::string> temp;
                                                    temp = split(currentString, " ");
                                                    if (currentClientInfo == temp[0] + " " + temp[1]) {
                                                        copyVector.push_back(temp[0] + " " + temp[1] + " " + toRegisterFile);
                                                    } else {
                                                        copyVector.push_back(temp[0] + " " + temp[1] + " " + temp[2]);
                                                    }
                                                }
                                                fromRegisterFile.close();
                                            }
                                            std::ofstream toRegisterFile;
                                            toRegisterFile.open("registered.txt");
                                            for(unsigned int i = 0; i < copyVector.size(); i++) {
                                                toRegisterFile<< copyVector[i] + "\n";
                                            }
                                            toRegisterFile.close();
                                        }
                                    }
                                } else {
                                    char registerFirst[DEFAULT_BUFLEN] = "You have to register first";
                                    sendn(ClientSocket, registerFirst, strlen(registerFirst));
                                }
                            }
                        }
                    }
                }
            }
        } while (readed > 0);
        _endthreadex( 0 );
        return 0;

}

unsigned int __stdcall acceptThreadFunction(void* pArguments) { //Поток для принятия клиентов
        SOCKET ClientSocket = INVALID_SOCKET;
        SOCKET ListenSocket = *(SOCKET*) pArguments;
        HANDLE myThreadHandlers[255];
        unsigned threadId;
        int connectionsCounter = 0;
        while(true){
            int ClientSocket = accept(ListenSocket, NULL, NULL); //2) Обработка запросов на подключение по этому порту от клиентов
            if (ClientSocket == INVALID_SOCKET) {
                 printf("accept failed with error: %d\n", WSAGetLastError());
                 closesocket(ListenSocket);
                 break;
            }
            WaitForSingleObject(mainThreadMutex, INFINITE);
            poolOfSockets.push_back(std::make_pair(connectionsCounter, ClientSocket));
            myThreadHandlers[connectionsCounter] = ((HANDLE)_beginthreadex(NULL, 0, &threadedFunction, (void*) &ClientSocket, 0, &threadId)); // 3) Поддержка одновременной работы нескольких клиентов через механизм нитей
            printf("new client connected with id: %d\n", connectionsCounter);
            connectionsCounter ++;
            ReleaseMutex(mainThreadMutex);
        }
        printf("closing all connections. Ending all threads\n");
        for (int i = 0; i < poolOfSockets.size(); i ++) {
            shutdown(poolOfSockets[i].second, 2);
            closesocket(poolOfSockets[i].second);
        }
        poolOfSockets.clear();
        WaitForMultipleObjects(connectionsCounter, myThreadHandlers, true, INFINITE);
        for (int i = 0; i < connectionsCounter; i++) {
            CloseHandle(myThreadHandlers[i]);
        }
        _endthreadex( 0 );
        return 0;

}
int main(void)
{
        WSADATA wsaData;
        int iResult;

        SOCKET ListenSocket = INVALID_SOCKET;
        HANDLE acceptThreadHandler;

        struct addrinfo *result = NULL;
        struct addrinfo hints;

        unsigned acceptThreadId;

        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;

        iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (iResult != 0) {
            printf("WSAStartup failed with error: %d\n", iResult);
            return 1;
        }

        iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
        if ( iResult != 0 ) {
            printf("getaddrinfo failed with error: %d\n", iResult);
            WSACleanup();
            return 1;
        }

        ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol); //1) Прослушивание определенного порта
        if (ListenSocket == INVALID_SOCKET) {
                printf("socket failed with error: %d\n", WSAGetLastError());
                freeaddrinfo(result);
                WSACleanup();
                return 1;
        }
        iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            printf("bind failed with error: %d\n", WSAGetLastError());
            freeaddrinfo(result);
            WSACleanup();
            closesocket(ListenSocket);
            return 1;
        }
        iResult = listen(ListenSocket, SOMAXCONN);
        if (iResult == SOCKET_ERROR) {
            printf("listen failed with error: %d\n", WSAGetLastError());
            WSACleanup();
            closesocket(ListenSocket);
            return 1;
        }

        acceptThreadHandler = (HANDLE)_beginthreadex(NULL, 0, &acceptThreadFunction, (void*) &ListenSocket, 0, &acceptThreadId);
        std::string endString = "end";
        std::string closeClientString = "close";
        std::string sendToClientString = "send";
        std::string showClientsString = "show";

        mainThreadMutex = CreateMutex( NULL, FALSE, NULL);

        while (true) {
            std::string str;
            int num = -1;
            printf("---------------\nChoose an operation:\n 1) end\n 2) close\n 3) send\n 4) show\n---------------\n");
            std::getline(std::cin, str);
            if (str == endString) {
                printf("Closing server \n");
                closesocket(ListenSocket);
                WaitForSingleObject(acceptThreadHandler, INFINITE );
                CloseHandle(acceptThreadHandler);
                break;
            } else {
                if (str == closeClientString) {//9) Принудительное отключение клиента
                    printf("---------------\nWrite an id of a client to disconnect\n---------------\n");
                    //scanf("%d", &num);
                    std::string strNum;
                    std::getline(std::cin, strNum);
                    num = atoi(strNum.c_str());
                    WaitForSingleObject(mainThreadMutex, INFINITE);
                    int indexToDelete = -1;
                    for(unsigned int i = 0; i < poolOfSockets.size(); i++) {
                        if (poolOfSockets[i].first == num) {
                            indexToDelete = i;
                            break;
                        }
                    }
                    if (indexToDelete != -1) {
                        shutdown(poolOfSockets[indexToDelete].second, 2);
                        closesocket(poolOfSockets[indexToDelete].second);
                        poolOfSockets.erase(poolOfSockets.begin() + indexToDelete);
                    }
                    ReleaseMutex(mainThreadMutex);
                } else {
                    if (str == sendToClientString) {
                        printf("---------------\nWrite an id of a client to send a message\n---------------\n");
                        std::string numStr;
                        std::getline(std::cin, numStr);
                        num = atoi(numStr.c_str());
                        char sendbuf[DEFAULT_BUFLEN];
                        printf("Your message: ");
                        std::string sendString;
                        std::getline(std::cin, sendString);
                        WaitForSingleObject(mainThreadMutex, INFINITE);
                        SOCKET sendSock;
                        for(unsigned int i = 0; i < poolOfSockets.size(); i++) {
                            if(poolOfSockets[i].first == num) {
                                sendSock = poolOfSockets[i].second;
                            }
                        }

                        int iSendResult ;
                        iSendResult = sendn(sendSock, (char*) sendString.c_str(), sendString.length());
                        if (iSendResult == SOCKET_ERROR) {
                            printf("send failed with error: %d\n", WSAGetLastError());
                            printf("deleting socket from pool of sockets \n");
                            shutdown(sendSock, 2);
                            closesocket(sendSock);
                        }
                        ReleaseMutex(mainThreadMutex);
                    } else {
                        if (str == showClientsString) {
                            printf("Available clients: ");
                            WaitForSingleObject(mainThreadMutex, INFINITE);
                            for(unsigned int i = 0; i < poolOfSockets.size(); i++) {
                                printf("%d, ", poolOfSockets[i].first);
                            }
                            printf("\n");
                            ReleaseMutex(mainThreadMutex);
                        }
                    }
                }
            }
        }
        WSACleanup();
        return 0;
}
