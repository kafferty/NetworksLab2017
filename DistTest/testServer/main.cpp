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

#define REGISTRATION_FILE "registered.txt"
#define NO_INFO "noInfo"
#define END_STRING "end"
#define CLOSE_CLIENT_STRING "close"
#define SEND_TO_CLIENT_STRING "send"
#define SHOW_CLIENTS_STRING "show"
#define REGISTER_STRING "register"
#define SHOW_TESTS_STRING "show"
#define GET_TEST_RESULT "getResult"
#define GET_TEST "getTest"
#define CHOOSE_OPERATION "---------------\nChoose an operation:\n 1) end\n 2) close\n 3) send\n 4) show\n---------------\n"
#define CLIENT_DISCONNECT "---------------\nWrite an id of a client to disconnect\n---------------\n"
#define CLIENT_SEND_MESSAGE "---------------\nWrite an id of a client to send a message\n---------------\n"
#define AVAILABLE_CLIENTS "Available clients: "
#define YOUR_MESSAGE "Your message: "
#define CLOSING_SERVER "Closing server \n"

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

        SOCKET clientSocket = *(SOCKET*) pArguments;

        int readed;
        do {
            char recvbuf[DEFAULT_BUFLEN];
            int recvbuflen = DEFAULT_BUFLEN;
            int iSendResult;
            int iResult;

            readed = readn(clientSocket, recvbuf, recvbuflen);
            if (readed == -1 || readed == 0) {
                shutdown(clientSocket, 2);
                closesocket(clientSocket);
                break;
            }
            printf("Bytes read: %d\n", readed);
            printf("Received data: %s\n", recvbuf);
            if (recvbuf == END_STRING) { //8) Обработка запроса на отключение клиента
                shutdown(clientSocket, 2);
                closesocket(clientSocket);
                int deleteSock;
                WaitForSingleObject(mainThreadMutex, INFINITE);
                SOCKET sockToDelete;

                for(unsigned int i = 0; i < poolOfSockets.size(); i++) {
                    if (poolOfSockets[i].second == clientSocket) {
                        deleteSock = i;
                        break;
                    }
                }
                poolOfSockets.erase(poolOfSockets.begin() + deleteSock);
                ReleaseMutex(mainThreadMutex);
                break;
            } else {
                if (std::string(recvbuf) == REGISTER_STRING) { //4) Регистрация клиента
                    std::vector<std::string> loginVector;
                    std::ifstream fromRegisterFile;
                    fromRegisterFile.open(REGISTRATION_FILE);
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
                    registerFile.open(REGISTRATION_FILE, std::ios::app);
                    while(true) {
                        readed = readn(clientSocket, recvbuf, recvbuflen);
                        if (readed < 0) {
                            break;
                        }
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
                                        loggedInClients.push_back(std::make_pair(clientSocket, infoString));
                                        sendn(clientSocket, successfulLogin, strlen(successfulLogin));
                                        flag = true;
                                    } else {
                                        char alreadyOnline[DEFAULT_BUFLEN] = "This account is online!";
                                        sendn(clientSocket, alreadyOnline, strlen(alreadyOnline));
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
                                        sendn(clientSocket, badLogin, strlen(badLogin));
                                        noLogin = true;//есть логин
                                        break;
                                    }
                                }
                                if (noLogin == false) {//если нет такого логина
                                    bool reLogin = false;
                                    for(unsigned int i = 0; i < loggedInClients.size(); i++) {//если клиент пытается зайти за другого юзера
                                        if (loggedInClients[i].first == clientSocket) {
                                            loggedInClients[i].second = infoString;
                                            reLogin = true;
                                            break;
                                        }
                                    }
                                    if (!reLogin) {
                                        loggedInClients.push_back(std::make_pair(clientSocket, infoString));
                                    }
                                    infoString = infoString + " no_result\n";
                                    registerFile << infoString;
                                    registerFile.close();
                                    char successfulRegistration[DEFAULT_BUFLEN] = "You are successfully registered";
                                    sendn(clientSocket, successfulRegistration, strlen(successfulRegistration));
                                    break;
                                }
                            }
                        } else {
                            char wrongNumber[DEFAULT_BUFLEN] = "Wrong number of argumenst";
                            sendn(clientSocket, wrongNumber, strlen(wrongNumber));
                        }
                    }
                } else {
                    if (recvbuf == SHOW_TESTS_STRING) { //4) Выдача клиенту списка тестов
                        char sendbuf[DEFAULT_BUFLEN] = "1 - Math test\n2 - Phsycological test\n";
                        sendn(clientSocket, sendbuf, strlen(sendbuf));
                    } else {
                        if (recvbuf == GET_TEST_RESULT) { //4) Выдача клиенту результата его последнего теста
                            std::string currentClientInfo = NO_INFO;
                            for(unsigned int i = 0; i < loggedInClients.size(); i++) {
                                if (loggedInClients[i].first == clientSocket) {
                                    currentClientInfo = loggedInClients[i].second;
                                    break;
                                }
                            }
                            if (currentClientInfo != NO_INFO) {//сюда не зайти, если не залогинен
                                std::ifstream fromRegisterFile;
                                fromRegisterFile.open(REGISTRATION_FILE);
                                std::string currentString;
                                if (fromRegisterFile.is_open()) {
                                    while(getline(fromRegisterFile, currentString)) {
                                        std::vector<std::string> temp;
                                        temp = split(currentString, " ");
                                        if (currentClientInfo == temp[0] + " " + temp[1]) {
                                            char yourResult[DEFAULT_BUFLEN] = "Your last result was: ";
                                            strcat(yourResult, temp[2].c_str());
                                            sendn(clientSocket, yourResult, strlen(yourResult));
                                        }
                                    }
                                    fromRegisterFile.close();
                                }
                            } else {
                                char registerFirst[DEFAULT_BUFLEN] = "You have to register first";
                                sendn(clientSocket, registerFirst, strlen(registerFirst));
                            }

                        } else { //5) Получение от клиента номера теста
                            std::vector<std::string> splitVect = split(std::string(recvbuf), " ");
                            if ((splitVect.size() > 1) && (splitVect[0] == getTest)) {
                                std::string currentClientInfo = NO_INFO;
                                for(unsigned int i = 0; i < loggedInClients.size(); i++) {
                                    if (loggedInClients[i].first == clientSocket) {
                                        currentClientInfo = loggedInClients[i].second;
                                        break;
                                    }
                                }
                            if (currentClientInfo != NO_INFO) {
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
                                            sendn(clientSocket, buf, strlen(buf));
                                            if (std::getline(file, question)) {
                                                for (unsigned int i = 0; i <= question.size(); i++) {
                                                    buf[i] = question[i];
                                                }
                                                //6) Посылаем варианты ответов
                                                sendn(clientSocket, buf, strlen(buf));
                                            }
                                            readn(clientSocket, recvbuf, DEFAULT_BUFLEN); //6) Получение ответов на вопросы
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
                                        sendn(clientSocket, resultingString, strlen(resultingString));
                                        std::string toRegisterFile = std::string(conv) + "_out_of_" + std::string(numberOfQuest);
                                        std::string currentClientInfo = NO_INFO;
                                        for(unsigned int i = 0; i < loggedInClients.size(); i++) {
                                            if (loggedInClients[i].first == clientSocket) {
                                                currentClientInfo = loggedInClients[i].second;
                                                break;
                                            }
                                        }
                                        std::vector<std::string> copyVector; //Запись последнего результата в файл
                                        if (currentClientInfo != NO_INFO) {
                                            std::ifstream fromRegisterFile;
                                            fromRegisterFile.open(REGISTRATION_FILE);
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
                                            toRegisterFile.open(REGISTRATION_FILE);
                                            for(unsigned int i = 0; i < copyVector.size(); i++) {
                                                toRegisterFile<< copyVector[i] + "\n";
                                            }
                                            toRegisterFile.close();
                                        }
                                    }
                                } else {
                                    char registerFirst[DEFAULT_BUFLEN] = "You have to register first";
                                    sendn(clientSocket, registerFirst, strlen(registerFirst));
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
        SOCKET clientSocket = INVALID_SOCKET;
        SOCKET listenSocket = *(SOCKET*) pArguments;
        HANDLE myThreadHandlers[255];
        unsigned threadId;
        int connectionsCounter = 0;
        while(true){
            int clientSocket = accept(listenSocket, NULL, NULL); //2) Обработка запросов на подключение по этому порту от клиентов
            if (clientSocket == INVALID_SOCKET) {
                 printf("accept failed with error: %d\n", WSAGetLastError());
                 closesocket(listenSocket);
                 break;
            }
            WaitForSingleObject(mainThreadMutex, INFINITE);
            poolOfSockets.push_back(std::make_pair(connectionsCounter, clientSocket));
            myThreadHandlers[connectionsCounter] = ((HANDLE)_beginthreadex(NULL, 0, &threadedFunction, (void*) &clientSocket, 0, &threadId)); // 3) Поддержка одновременной работы нескольких клиентов через механизм нитей
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

        SOCKET listenSocket = INVALID_SOCKET;
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

        listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol); //1) Прослушивание определенного порта
        if (listenSocket == INVALID_SOCKET) {
                printf("socket failed with error: %d\n", WSAGetLastError());
                freeaddrinfo(result);
                WSACleanup();
                return 1;
        }

        int yes=1;

        if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
        exit(1);
        }

        iResult = bind( listenSocket, result->ai_addr, (int)result->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            printf("bind failed with error: %d\n", WSAGetLastError());
            freeaddrinfo(result);
            WSACleanup();
            closesocket(listenSocket);
            return 1;
        }
        iResult = listen(listenSocket, SOMAXCONN);
        if (iResult == SOCKET_ERROR) {
            printf("listen failed with error: %d\n", WSAGetLastError());
            WSACleanup();
            closesocket(listenSocket);
            return 1;
        }

        acceptThreadHandler = (HANDLE)_beginthreadex(NULL, 0, &acceptThreadFunction, (void*) &listenSocket, 0, &acceptThreadId);

        mainThreadMutex = CreateMutex( NULL, FALSE, NULL);

        while (true) {
            std::string str;
            int num = -1;
            printf(CHOOSE_OPERATION);
            std::getline(std::cin, str);
            if (str == END_STRING) {
                printf(CLOSING_SERVER);
                closesocket(listenSocket);
                WaitForSingleObject(acceptThreadHandler, INFINITE );
                CloseHandle(acceptThreadHandler);
                break;
            } else {
                if (str == CLOSE_CLIENT_STRING) {//9) Принудительное отключение клиента
                    printf(CLIENT_DISCONNECT);
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
                    if (str == SEND_TO_CLIENT_STRING) {
                        printf(CLIENT_SEND_MESSAGE);
                        std::string numStr;
                        std::getline(std::cin, numStr);
                        num = atoi(numStr.c_str());
                        char sendbuf[DEFAULT_BUFLEN];
                        printf(YOUR_MESSAGE);
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
                        if (str == SHOW_CLIENTS_STRING) {
                            printf(AVAILABLE_CLIENTS);
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
