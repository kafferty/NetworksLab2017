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


std::vector<std::string> split(std::string s, std::string delimiter){ //Разбивает строку по делиметру
    std::vector<std::string> list;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        list.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    list.push_back(s);
    return list;
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
            if (readed == -1) {
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
                for(int i = 0; i < poolOfSockets.size(); i++) {
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
                        bool flag = false;
                        for(int i = 0; i < loginVector.size(); i++) {
                            if (loginVector[i] == infoString) {
                                std::string successfulLogin = "You are successfully logged in!";
                                send(ClientSocket, successfulLogin.c_str(), DEFAULT_BUFLEN, 0);
                                flag = true;
                                break;
                            }
                        }
                        if (flag == true)
                            break;
                        else {
                            bool noLogin = false;
                            for(int i = 0; i < loginVector.size(); i++) {
                                if (split(infoString, " ")[0] == split(loginVector[i], " ")[0]) {
                                    std::string badLogin = "Try another password or login!";
                                    send(ClientSocket, badLogin.c_str(),  DEFAULT_BUFLEN, 0);
                                    noLogin = true;
                                    break;
                                }
                            }
                            if (noLogin == false) {
                                infoString = infoString + " no_result\n";
                                registerFile << infoString;
                                registerFile.close();
                                std::string successfulRegistration = "You are successfully registered";
                                send(ClientSocket, successfulRegistration.c_str(), DEFAULT_BUFLEN, 0);
                                break;
                            }
                        }
                    }
                } else {
                    if (recvbuf == showTestsString) { //4) Выдача клиенту списка тестов
                        char sendbuf[DEFAULT_BUFLEN] = "1 - Math test\n2 - Phsycological test\n";
                        send(ClientSocket, sendbuf, DEFAULT_BUFLEN, 0);
                    } else {
                        if (recvbuf == getTestResult) { //4) Выдача клиенту результата его последнего теста
                            //TODO
                        } else { //5) Получение от клиента номера теста
                            std::vector<std::string> splitVect = split(std::string(recvbuf), " ");
                            if ((splitVect.size() > 1) && (splitVect[0] == getTest)) {
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
                                        for (int i = 0; i <= question.size(); i++) {
                                            buf[i] = question[i];
                                        }
                                        send(ClientSocket, buf, DEFAULT_BUFLEN, 0); //6) Посылаем вопрос
                                        if (std::getline(file, question)) {
                                            for (int i = 0; i <= question.size(); i++) {
                                                buf[i] = question[i];
                                            }
                                            send(ClientSocket, buf, DEFAULT_BUFLEN,0); //6) Посылаем варианты ответов
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
                                    std::string resultingString = "Your result is " + std::string(conv) + " out of " + std::string(numberOfQuest);
                                    send(ClientSocket, resultingString.c_str(), DEFAULT_BUFLEN, 0); // 7) После прохождения теста - выдача клиенту его результата
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
            ClientSocket = accept(ListenSocket, NULL, NULL); //2) Обработка запросов на подключение по этому порту от клиентов
            if (ClientSocket == INVALID_SOCKET) {
                 printf("accept failed with error: %d\n", WSAGetLastError());
                 closesocket(ListenSocket);
                 break;
            }
            WaitForSingleObject(mainThreadMutex, INFINITE);
            SOCKET *nClientSocket = new SOCKET;
            *nClientSocket = ClientSocket;
            poolOfSockets.push_back(std::make_pair(connectionsCounter, ClientSocket));
            myThreadHandlers[connectionsCounter] = ((HANDLE)_beginthreadex(NULL, 0, &threadedFunction, (void*) nClientSocket, 0, &threadId)); // 3) Поддержка одновременной работы нескольких клиентов через механизм нитей
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
        SOCKET *nListenSocket = new SOCKET;
        *nListenSocket = ListenSocket;
        acceptThreadHandler = (HANDLE)_beginthreadex(NULL, 0, &acceptThreadFunction, (void*) nListenSocket, 0, &acceptThreadId);
        std::string endString = "end";
        std::string closeClientString = "close";
        std::string sendToClientString = "send";
        std::string showClientsString = "show";
        mainThreadMutex = CreateMutex( NULL, FALSE, NULL);
        while (true) {
            char str[255];
            int num = -1;
            printf("---------------\nChoose an operation:\n 1) end\n 2) close\n 3) send\n 4) show\n---------------\n");
            scanf("%s", str);
            if (str == endString) {
                printf("Closing server \n");
                closesocket(ListenSocket);
                WaitForSingleObject(acceptThreadHandler, INFINITE );
                CloseHandle(acceptThreadHandler);
                break;
            } else {
                if (str == closeClientString) {//9) Принудительное отключение клиента
                    printf("---------------\nWrite an id of a client to disconnect\n---------------\n");
                    scanf("%d", &num);

                    WaitForSingleObject(mainThreadMutex, INFINITE);
                    int indexToDelete;
                    for(int i = 0; i < poolOfSockets.size(); i++) {
                        if (poolOfSockets[i].first == num) {
                            indexToDelete = i;
                            break;
                        }
                    }
                    shutdown(poolOfSockets[indexToDelete].second, 2);
                    closesocket(poolOfSockets[indexToDelete].second);
                    poolOfSockets.erase(poolOfSockets.begin() + indexToDelete);
                    ReleaseMutex(mainThreadMutex);
                } else {
                    if (str == sendToClientString) {
                        printf("---------------\nWrite an id of a client to send a message\n---------------\n");
                        scanf("%d", &num);
                        fflush(stdin);
                        char sendbuf[DEFAULT_BUFLEN];
                        printf("Your message: ");
                        fgets(sendbuf, DEFAULT_BUFLEN, stdin);
                        printf("%s\n", sendbuf);
                        WaitForSingleObject(mainThreadMutex, INFINITE);
                        SOCKET sendSock;
                        for(int i = 0; i < poolOfSockets.size(); i++) {
                            if(poolOfSockets[i].first == num) {
                                sendSock = poolOfSockets[i].second;
                            }
                        }

                        int iSendResult ;
                        iSendResult = send(sendSock, sendbuf, DEFAULT_BUFLEN, 0);
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
                            for(int i = 0; i < poolOfSockets.size(); i++) {
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
