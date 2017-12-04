#undef UNICODE
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fstream>
#include <algorithm>
#include <cstring>


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27021"

#define REGISTRATION_FILE "registered.txt"
#define NO_INFO "noInfo"
#define SHOW_CLIENTS_STRING "show"
#define REGISTER_STRING "register"

#define CHOOSE_OPERATION "---------------\nChoose an operation:\n 1) end\n 2) close\n 3) show\n---------------\n"
#define CLIENT_DISCONNECT "---------------\nWrite an id of a client to disconnect\n---------------\n"
#define AVAILABLE_CLIENTS "Available clients: "
#define CLOSING_SERVER "Closing server \n"
#define SUCCESSFUL_LOGIN "You are successfully logged in!"
#define ALREADY_ONLINE "This account is online!"
#define BAD_LOGIN "Try another password or login!"
#define SUCCESSFUL_REGISTERED "You are successfully registered"
#define WRONG_NUMBER "Wrong number of argumenеts. You can't create login and password with spaces"
#define YOUR_RESULT "Your last result was: "
#define REGISTER_FIRST "You have to register first"
#define YOUR_NEW_RESULT "Your result is "
#define CLOSING_CONNECT "closing all connections. Ending all threads\n"
#define DELETE_SOCKET "deleting socket from pool of sockets \n"

const std::string END_STRING = "end";
const std::string CLOSE_CLIENT_STRING = "close";
const std::string GET_TEST_RESULT = "getResult";
const std::string GET_TEST = "getTest";
const std::string SHOW_TESTS_STRING = "show";
const std::string ANSWER = "ANSWERS:";
const std::string QUESTION = "QUESTION:";
const std::string Q_END = "END:";

std::vector< std::pair<int, int> > poolOfSockets; //Вектор пар для клиентов
pthread_mutex_t mainThreadMutex;
std::vector< std::pair<int, std::string> > loggedInClients;

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

int sendn(int sendSocket, char *buffer, int n, sockaddr_in *from, socklen_t fromlen) { // Функция, посылающая >= n байт
   int bytesSent = 0;
   while(bytesSent < n) {
       std::string num = std::to_string(bytesSent / DEFAULT_BUFLEN) + " ";
       for(int i = 0; i < DEFAULT_BUFLEN; i++) {
           num.append(buffer);
           buffer = buffer + 1;
       }
       ssize_t sendResult = sendto(sendSocket, num.c_str(), 2 * DEFAULT_BUFLEN , 0, (sockaddr *)from, fromlen);
       char packetNum[DEFAULT_BUFLEN];
       ssize_t recvResult = recvfrom(sendSocket, packetNum, 2 * DEFAULT_BUFLEN, 0, (sockaddr *)from, &fromlen);
       if (atoi(packetNum) != bytesSent / DEFAULT_BUFLEN) return -2;
       //int sendResult = send(sendSocket, buffer, DEFAULT_BUFLEN, 0);
       if (sendResult == -1) {
           return -1;
       }
       //buffer = buffer + DEFAULT_BUFLEN;
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

void * threadedFunction(void* pArguments) {

    int clientSocket = *(int*) pArguments;
    struct sockaddr_in from{};
    socklen_t fromlen = sizeof(from);

    ssize_t readed;
    do {
        char recvbf[DEFAULT_BUFLEN];
        int recvbuflen = DEFAULT_BUFLEN;

        readed = recvfrom(clientSocket, recvbf, 2 * static_cast<size_t>(recvbuflen), 0, (sockaddr *)(&from), &fromlen);
        if (readed == -1 || readed == 0) {
            shutdown(clientSocket, 2);
            close(clientSocket);
            break;
        }
        std::vector<std::string> vec = split(std::string(recvbf), " ");
        sendn(clientSocket, const_cast<char *>(("0 " + vec[0]).c_str()), DEFAULT_BUFLEN, &from, fromlen);
        std::string recvbuf;
        for (int i = 1; i < vec.size(); i++)
            recvbuf.append(vec[i]);

        printf("Bytes read: %d\n", (int) readed);
        std::cout << recvbuf << std::endl;
        if (recvbuf == END_STRING) { //8) Обработка запроса на отключение клиента
            shutdown(clientSocket, 2);
            close(clientSocket);
            int deleteSock = -1;
            pthread_mutex_lock(&mainThreadMutex);

            for(unsigned int i = 0; i < poolOfSockets.size(); i++) {
                if (poolOfSockets[i].second == clientSocket) {
                    deleteSock = i;
                    break;
                }
            }
            if (deleteSock != -1) poolOfSockets.erase(poolOfSockets.begin() + deleteSock);
            pthread_mutex_unlock(&mainThreadMutex);
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
                    readed = recvfrom(clientSocket, recvbf, 2 * static_cast<size_t>(recvbuflen), 0, (sockaddr *)(&from), &fromlen);
                    if (readed < 0) {
                        break;
                    }
                    std::vector<std::string> temp = split(std::string(recvbf), " ");
                    sendn(clientSocket, const_cast<char *>(("0 " + temp[0]).c_str()), DEFAULT_BUFLEN, &from, fromlen);
                    recvbuf = "";
                    for (int i = 1; i < temp.size(); i++)
                        recvbuf.append(temp[i]);

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
                                    char successfulLogin[DEFAULT_BUFLEN] = SUCCESSFUL_LOGIN;
                                    loggedInClients.emplace_back(std::make_pair(clientSocket, infoString));
                                    sendn(clientSocket, successfulLogin, static_cast<int>(strlen(successfulLogin)), &from, fromlen);
                                    flag = true;
                                } else {
                                    char alreadyOnline[DEFAULT_BUFLEN] = ALREADY_ONLINE;
                                    sendn(clientSocket, alreadyOnline, static_cast<int>(strlen(alreadyOnline)), &from, fromlen);
                                }
                                break;
                            }
                        }
                        if (flag)
                            break;
                        else {
                            bool noLogin = false;
                            for(unsigned int i = 0; i < loginVector.size(); i++) {
                                if (split(infoString, " ")[0] == split(loginVector[i], " ")[0]) {
                                    char badLogin[DEFAULT_BUFLEN] = BAD_LOGIN;
                                    sendn(clientSocket, badLogin, static_cast<int>(strlen(badLogin)), &from, fromlen);
                                    noLogin = true;//есть логин
                                    break;
                                }
                            }
                            if (!noLogin) {//если нет такого логина
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
                                char successfulRegistration[DEFAULT_BUFLEN] = SUCCESSFUL_REGISTERED;
                                sendn(clientSocket, successfulRegistration,
                                      static_cast<int>(strlen(successfulRegistration)), &from, fromlen);
                                break;
                            }
                        }
                    } else {
                        char wrongNumber[DEFAULT_BUFLEN] = WRONG_NUMBER;
                        sendn(clientSocket, wrongNumber, static_cast<int>(strlen(wrongNumber)), &from, fromlen);
                    }
                }
            } else {
                if (recvbuf == SHOW_TESTS_STRING) { //4) Выдача клиенту списка тестов
                    std::ifstream file;
                    std::string nameFile = "list_tests.txt";
                    file.open (nameFile.c_str());
                    if (file.is_open()) {
                        std::string listTests = "";
                        std::string test;

                        while (std::getline(file, test)) {
                            listTests = listTests + test + "\n";
                        }
                        file.close();
                        sendn(clientSocket, const_cast< char* >(listTests.c_str()),
                              static_cast<int>(listTests.length()), &from, fromlen);
                    }
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
                                        char yourResult[DEFAULT_BUFLEN] = YOUR_RESULT;
                                        strcat(yourResult, temp[2].c_str());
                                        sendn(clientSocket, yourResult, static_cast<int>(strlen(yourResult)), &from, fromlen);
                                    }
                                }
                                fromRegisterFile.close();
                            }
                        } else {
                            char registerFirst[DEFAULT_BUFLEN] = REGISTER_FIRST;
                            sendn(clientSocket, registerFirst, static_cast<int>(strlen(registerFirst)), &from, fromlen);
                        }

                    } else { //5) Получение от клиента номера теста
                        std::vector<std::string> splitVect = split(std::string(recvbuf), " ");

                        if ((splitVect.size() > 1) && (splitVect[0] == GET_TEST)) {
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
                                    std::vector<std::string> askVector;
                                    std::getline(file, question);
                                    while(question != Q_END) {
                                        if(question == QUESTION) {
                                            std::getline(file, question);
                                            sendn(clientSocket, const_cast<char *>(question.c_str()),
                                                  static_cast<int>(question.length()), &from, fromlen);
                                            std::getline(file, question);
                                        } else if (question == ANSWER) {
                                            std::getline(file, question);
                                            std::string ansToSend = "";
                                            while (question != QUESTION && question != Q_END) {
                                                ansToSend = ansToSend + question + "\n";
                                                std::getline(file, question);
                                            }
                                            std::cout<<ansToSend;
                                            char buf[ansToSend.length()];
                                            for (int i = 0; i < ansToSend.length(); i++) {
                                                buf[i] = ansToSend[i];
                                            }
                                            buf[ansToSend.length()] = '\0';
                                            sendn(clientSocket, buf, static_cast<int>(strlen(buf)), &from, fromlen);
                                            recvfrom(clientSocket, recvbf, 2 * DEFAULT_BUFLEN, 0,
                                                     (sockaddr *) (&from), &fromlen); //6) Получение ответов на вопросы
                                            std::vector<std::string> temp = split(std::string(recvbf), " ");
                                            sendn(clientSocket, const_cast<char *>(("0 " + temp[0]).c_str()), DEFAULT_BUFLEN, &from, fromlen);
                                            recvbuf = "";
                                            for (int i = 1; i < temp.size(); i++)
                                                recvbuf.append(temp[i]);
                                            vec.emplace_back(std::string(recvbuf));
                                        }
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
                                    sprintf(conv, "%d", summary);
                                    char numberOfQuest[DEFAULT_BUFLEN];
                                    sprintf(numberOfQuest, "%d", static_cast<int>(vec.size()));
                                    char resultingString[DEFAULT_BUFLEN] = YOUR_NEW_RESULT;
                                    std::string toConcat = std::string(conv) + " out of " + std::string(numberOfQuest);
                                    strcat(resultingString, toConcat.c_str());
                                    sendn(clientSocket, resultingString, static_cast<int>(strlen(resultingString)), &from, fromlen);
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
                                char registerFirst[DEFAULT_BUFLEN] = REGISTER_FIRST;
                                sendn(clientSocket, registerFirst, static_cast<int>(strlen(registerFirst)), &from, fromlen);
                            }
                        }
                    }
                }
            }
        }
    } while (readed > 0);

}

void * acceptThreadFunction(void* pArguments) { //Поток для принятия клиентов
    int listenSocket = *(int*) pArguments;
    std::vector< pthread_t > myThreadHandlers;
    int connectionsCounter = 0;
    int portnum = 2000;

    while(true){
        char recvbuf[2 * DEFAULT_BUFLEN];
        struct sockaddr_in from{};
        socklen_t fromlen = sizeof(from);
        ssize_t recvBytes = recvfrom(listenSocket, recvbuf, 2 * DEFAULT_BUFLEN, 0, (sockaddr *)(&from), &fromlen);
        std::cout<<std::string(recvbuf)<<std::endl;
        std::vector<std::string> temp = split(std::string(recvbuf), " ");
        sendn(listenSocket, const_cast<char *>(("0 " + temp[0]).c_str()), DEFAULT_BUFLEN, &from, fromlen);

        if (recvBytes <= 0) break;
        struct addrinfo *result = nullptr;
        struct addrinfo hints{};

        bzero(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        hints.ai_flags = AI_PASSIVE;

        int iResult = getaddrinfo(nullptr, std::to_string(portnum).c_str(), &hints, &result);
        if (iResult != 0) {
            std::cout << "getaddrinfo failed with error "<< std::endl;
        }

        int eachConnectionSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (eachConnectionSocket == -1) {
            std::cout << "socket failed with error\n";
            freeaddrinfo(result);
        }
        iResult = bind(eachConnectionSocket, result->ai_addr, (int) result->ai_addrlen);
        if (iResult == -1) {
            std::cout << "bind failed with error \n";
            freeaddrinfo(result);
            shutdown(eachConnectionSocket, 2);
            close(eachConnectionSocket);
        }
        ssize_t sendBytes = sendn(listenSocket, const_cast<char *>(std::to_string(portnum).c_str()), DEFAULT_BUFLEN, &from, fromlen);
        pthread_mutex_lock(&mainThreadMutex);
        poolOfSockets.emplace_back(std::make_pair(connectionsCounter, eachConnectionSocket));
        pthread_t workingThread;
        pthread_create(&workingThread, nullptr, &threadedFunction, (void*) &eachConnectionSocket);
        myThreadHandlers.emplace_back(workingThread);
        printf("new client connected with id: %d\n", connectionsCounter);
        connectionsCounter ++;
        portnum += 1;
        pthread_mutex_unlock(&mainThreadMutex);
    }
    printf(CLOSING_CONNECT);
    for (int i = 0; i < poolOfSockets.size(); i ++) {
        shutdown(poolOfSockets[i].second, 2);
        close(poolOfSockets[i].second);
    }
    poolOfSockets.clear();
    for (int i = 0; i < myThreadHandlers.size(); i++) {
        pthread_join(myThreadHandlers[i], nullptr);
    }

}
int main()
{
    int iResult;

    int listenSocket = -1;
    pthread_t acceptThreadHandler;

    struct addrinfo *result = nullptr;
    struct addrinfo hints{};


    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;


    iResult = getaddrinfo(nullptr, DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        return 1;
    }

    listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol); //1) Прослушивание определенного порта
    if (listenSocket == -1) {
        printf("socket failed with error:\n");
        freeaddrinfo(result);
        return 1;
    }

    int yes=1;

    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    iResult = bind( listenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == -1) {
        printf("bind failed with error: \n");
        freeaddrinfo(result);
        close(listenSocket);
        return 1;
    }

    pthread_create(&acceptThreadHandler, nullptr, &acceptThreadFunction, (void*) &listenSocket);

    pthread_mutex_init(&mainThreadMutex, nullptr);

    while (true) {
        std::string str;
        int num = -1;
        printf(CHOOSE_OPERATION);
        std::getline(std::cin, str);
        if (str == END_STRING) {
            printf(CLOSING_SERVER);
            shutdown(listenSocket, 2);
            close(listenSocket);
            pthread_join(acceptThreadHandler, nullptr);
            break;
        } else {
            if (str == CLOSE_CLIENT_STRING) {//9) Принудительное отключение клиента
                printf(CLIENT_DISCONNECT);
                std::string strNum;
                std::getline(std::cin, strNum);
                num = atoi(strNum.c_str());
                pthread_mutex_lock(&mainThreadMutex);
                int indexToDelete = -1;
                for(unsigned int i = 0; i < poolOfSockets.size(); i++) {
                    if (poolOfSockets[i].first == num) {
                        indexToDelete = i;
                        break;
                    }
                }
                if (indexToDelete != -1) {
                    shutdown(poolOfSockets[indexToDelete].second, 2);
                    close(poolOfSockets[indexToDelete].second);
                    poolOfSockets.erase(poolOfSockets.begin() + indexToDelete);
                }
                pthread_mutex_unlock(&mainThreadMutex);
            } else {
                if (str == SHOW_CLIENTS_STRING) {
                    printf(AVAILABLE_CLIENTS);
                    pthread_mutex_lock(&mainThreadMutex);
                    for(unsigned int i = 0; i < poolOfSockets.size(); i++) {
                        printf("%d, ", poolOfSockets[i].first);
                    }
                    printf("\n");
                    pthread_mutex_unlock(&mainThreadMutex);
                }
            }
        }
    }
    return 0;
}