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
#include <unordered_map>


#define DEFAULT_BUFLEN 20
#define DEFAULT_PORT "27021"

#define REGISTRATION_FILE "registered.txt"
#define NO_INFO "noInfo"
#define SHOW_CLIENTS_STRING "show"
#define REGISTER_STRING "register"

#define CHOOSE_OPERATION "---------------\nChoose an operation:\n 1) end\n---------------\n"
#define CLOSING_SERVER "Closing server \n"
#define SUCCESSFUL_LOGIN "You are successfully logged in!"
#define ALREADY_ONLINE "This account is online!"
#define BAD_LOGIN "Try another password or login!"
#define SUCCESSFUL_REGISTERED "You are successfully registered"
#define WRONG_NUMBER "Wrong number of argumenеts. You can't create login and password with spaces"
#define YOUR_RESULT "Your last result was: "
#define REGISTER_FIRST "You have to register first"
#define YOUR_NEW_RESULT "Your result is "


const std::string END_STRING = "end";
const std::string CLOSE_CLIENT_STRING = "close";
const std::string GET_TEST_RESULT = "getResult";
const std::string GET_TEST = "getTest";
const std::string SHOW_TESTS_STRING = "show";
const std::string ANSWER = "ANSWERS:";
const std::string QUESTION = "QUESTION:";
const std::string Q_END = "END:";

int registerClient(int clientSocket, int counterRecv, int counterSend, ssize_t readed, std::string recvbf, sockaddr_in from);

std::vector< std::pair<int, int> > poolOfSockets; //Вектор пар для клиентов
pthread_mutex_t mainThreadMutex;
std::vector< std::pair<int, std::string> > loggedInClients;
std::unordered_map<std::string, std::string> loggedClients;

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
ssize_t sendOneTime(int connectSocket, char * connectstr, sockaddr_in &server, std::string packetNumber) {
    socklen_t len = sizeof(server);
    char connectString[DEFAULT_BUFLEN] = "";
    strncat(connectString, (packetNumber + " ").c_str(), (packetNumber + " ").size());
    strncat(connectString, connectstr, 512 - (packetNumber + " ").size());
    std::cout << connectString << std::endl;
    ssize_t iSend = sendto(connectSocket, connectString, DEFAULT_BUFLEN, 0, (sockaddr *)(&server), len);
    return iSend;
}

int sendn(int sendSocket, char *buffer, size_t n, sockaddr_in &from, int &counter) { // Функция, посылающая >= n байт
    int bytesSent = 0;
    int finalCounter = 0;
    while(bytesSent < n) {
        std::string num = std::to_string( (bytesSent / (DEFAULT_BUFLEN - 2)) + counter ) + " ";
        sendOneTime(sendSocket, buffer, from, num);
        finalCounter = (bytesSent /(DEFAULT_BUFLEN - 2)) + counter;
        buffer += DEFAULT_BUFLEN - 2;
        bytesSent += DEFAULT_BUFLEN - 2;
    }
    counter = ++finalCounter;
    return bytesSent;
}

ssize_t recvOneTime(int connectSocket, std::string &final, size_t recvbuflen, sockaddr_in &server, std::string packetNumber) {
    char buf[DEFAULT_BUFLEN];
    socklen_t len = sizeof(server);
    ssize_t size = recvfrom(connectSocket, buf, recvbuflen, 0, (sockaddr *)(&server), &len);
    std::vector<std::string> recvVec = split(std::string(buf), " ");
    if (recvVec[0] != packetNumber) return -1;
    for(int i = 1; i < recvVec.size(); i++) {
        final.append(recvVec[i]);
        if (i == recvVec.size() - 1) break;
        final.append(" ");
    }
    return size;
}

int registerClient(int clientSocket, int counterRecv, int counterSend, ssize_t readed, std::string recvbf, sockaddr_in from) {
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
        char endFunc[DEFAULT_BUFLEN] = "FUNCTION END";
        sendOneTime(clientSocket, endFunc, from, std::to_string(counterSend++));
        counterRecv = 0;
        counterSend = 0;
        recvbf = "";
        readed = recvOneTime(clientSocket, recvbf, DEFAULT_BUFLEN, from, std::to_string(counterRecv ++));
        if (readed < 0) {
            break;
        }

        std::string infoString(recvbf);
        if(split(infoString, " ").size() == 2) {
            bool flag = false;//для выхода из цикла
            for(unsigned int i = 0; i < loginVector.size(); i++) {
                if (loginVector[i] == infoString) {
                    bool isOnline = false;//залогинен ли
                    for(unsigned int i = 0; i < loggedInClients.size(); i++) {
                        if (loggedInClients[i].second == infoString) {
                            isOnline = true;
                            break;
                        }
                    }
                    if (!isOnline) {
                        char successfulLogin[DEFAULT_BUFLEN] = SUCCESSFUL_LOGIN;
                        loggedInClients.emplace_back(std::make_pair(clientSocket, infoString));
                        sendOneTime(clientSocket, successfulLogin, from, std::to_string(counterSend ++));
                        flag = true;
                    } else {
                        char alreadyOnline[DEFAULT_BUFLEN] = ALREADY_ONLINE;
                        sendOneTime(clientSocket, alreadyOnline, from, std::to_string(counterSend ++));
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
                        sendOneTime(clientSocket, badLogin, from, std::to_string(counterSend ++));
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
                    sendOneTime(clientSocket, successfulRegistration, from, std::to_string(counterSend ++));
                    break;
                }
            }
        } else {
            char wrongNumber[DEFAULT_BUFLEN] = WRONG_NUMBER;
            sendOneTime(clientSocket, wrongNumber, from, std::to_string(counterSend ++));
        }
    }
    return counterSend;
}

int showListOfTests(int clientSocket, int counterSend, sockaddr_in from) {

    std::ifstream file;
    std::string nameFile = "list_tests.txt";
    file.open (nameFile.c_str());
    if (file.is_open()) {
        std::string listTests = "";
        std::string test;

        while (std::getline(file, test)) {
            listTests.append(test + "\n");
        }
        file.close();
        sendn(clientSocket, const_cast<char *>(listTests.c_str()), listTests.length(), from, counterSend);
    }
    return counterSend;
}

int startTest(int clientSocket, int counterRecv, int counterSend, std::string recvbuf, sockaddr_in from) {
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
            file.open(testString);
            if (file.is_open()) {
                std::vector<std::string> askVector;
                std::getline(file, question);
                while(question != Q_END) {
                    if(question == QUESTION) {
                        std::getline(file, question);
                        sendn(clientSocket, const_cast<char *>(question.c_str()),
                              static_cast<int>(question.length()), from, counterSend);
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
                        sendOneTime(clientSocket, buf, from, std::to_string(counterSend++));
                        std::string ansString;
                        char endFunc[DEFAULT_BUFLEN] = "FUNCTION END";
                        sendOneTime(clientSocket, endFunc, from, std::to_string(counterSend++));
                        counterRecv = 0;
                        counterSend = 0;
                        recvOneTime(clientSocket, ansString, DEFAULT_BUFLEN, from, std::to_string(counterRecv++));
                        vec.emplace_back(ansString);
                    }
                }


                file.close();

                std::ifstream ansFile;
                ansFile.open((ansTestString));
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
                sendOneTime(clientSocket, resultingString, from, std::to_string(counterSend++));
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
            sendOneTime(clientSocket, registerFirst, from, std::to_string(counterSend++));
        }
    }
    return counterSend;
}

int getResult(int clientSocket, int counterSend, sockaddr_in from) {
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
                    sendOneTime(clientSocket, yourResult, from, std::to_string(counterSend++));
                }
            }
            fromRegisterFile.close();
        }
    } else {
        char registerFirst[DEFAULT_BUFLEN] = REGISTER_FIRST;
        sendOneTime(clientSocket, registerFirst, from, std::to_string(counterSend++));
    }
    return counterSend;
}

void * workingFlow (void* pArguments) {

    int clientSocket = *(int*) pArguments;
    struct sockaddr_in from{};

    ssize_t readed;
    do {

        std::string recvbf;
        int counterRecv = 0;
        int counterSend = 0;
        if ((readed = recvOneTime(clientSocket, recvbf, DEFAULT_BUFLEN, from, std::to_string(counterRecv))) <= 0) {
            shutdown(clientSocket, 2);
            close(clientSocket);
            break;
        }
        std::string recvbuf = std::string(recvbf);
        std::cout << "Bytes read: " << readed << std::endl;
        std::cout << recvbuf << std::endl;
            if (std::string(recvbuf) == REGISTER_STRING) { //4) Регистрация клиента
                counterSend = registerClient(clientSocket, counterRecv, counterSend, readed, recvbf, from);
            } else {
                if (recvbuf == SHOW_TESTS_STRING) { //4) Выдача клиенту списка тестов
                    counterSend = showListOfTests(clientSocket, counterSend, from);
                } else {
                    if (recvbuf == GET_TEST_RESULT) { //4) Выдача клиенту результата его последнего теста
                        counterSend = getResult(clientSocket, counterSend, from);

                    } else { //5) Получение от клиента номера теста
                        counterSend = startTest(clientSocket, counterRecv, counterSend, recvbuf, from);
                    }
                }
            }
        char endFunc[DEFAULT_BUFLEN] = "FUNCTION END";
        sendOneTime(clientSocket, endFunc, from, std::to_string(counterSend++));
    } while (readed > 0);
}


int main()
{
    int iResult;

    int listenSocket;
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


    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&acceptThreadHandler, &attr, &workingFlow, &listenSocket);

    pthread_mutex_init(&mainThreadMutex, nullptr);

    while (true) {
        std::string str;
        printf(CHOOSE_OPERATION);
        std::getline(std::cin, str);
        if (str == END_STRING) {
            printf(CLOSING_SERVER);
            shutdown(listenSocket, 2);
            close(listenSocket);
            pthread_join(acceptThreadHandler, nullptr);
            break;
        }
    }
    return 0;
}