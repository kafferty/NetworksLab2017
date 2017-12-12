#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x501
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <pthread.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <strings.h>

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27021"

bool toFile = false;
std::ofstream file;
pthread_mutex_t recv_send_mutex;


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

int readn(SOCKET newsockfd, char *buffer, int n) {
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

ssize_t recvOneTime(SOCKET ConnectSocket, std::string &final, size_t recvbuflen, sockaddr_in server) {
    char buf[DEFAULT_BUFLEN];
    std::cout << "hey" << std::endl;
    socklen_t len = sizeof(server);
    pthread_mutex_lock(&recv_send_mutex);
    ssize_t size = recvfrom(ConnectSocket, buf, recvbuflen, 0, (sockaddr *)(&server), &len);
    char packet[3] = "0 ";

    std::string bufstring = std::string(buf);
    std::cout << bufstring << std::endl;
    std::vector<std::string> recvVec = split(bufstring, " ");
    strncat(packet, recvVec[0].c_str(), 512);
    sendto(ConnectSocket, packet, recvbuflen, 0, (sockaddr *)(&server), len);
    pthread_mutex_unlock(&recv_send_mutex);
    final = "";
    for(int i = 1; i < recvVec.size(); i++) final.append(recvVec[i]);
    return size;
}

int sendOneTime(SOCKET ConnectSocket, char * connectstr, int recvbuflen, sockaddr_in server) {
    std::string packet = "0";
    char connectString[3] = "0 ";
    int len = sizeof(server);
    char buf[DEFAULT_BUFLEN];
    strncat(connectString, connectstr, 512);
    pthread_mutex_lock(&recv_send_mutex);
    sendto(ConnectSocket, connectString, recvbuflen, 0, (sockaddr *)(&server), len);
    recvfrom(ConnectSocket, buf, recvbuflen, 0, (sockaddr *)(&server), &len);
    pthread_mutex_unlock(&recv_send_mutex);
    std::cout<<buf<<std::endl;
    std::vector<std::string> vec = split(std::string(buf), " ");
    for (int i = 0; i < vec.size(); i++) {
        std::cout <<"vec["<<i<<"] = {" << vec[i] <<"}\n";
    }
    if (vec[1] == "0") {
        std::cout << "returning true\n";
        return 0;
    }
    else return 1;
}

void * readFunc(void* pArguments) {
        SOCKET ConnectSocket = *(SOCKET*) pArguments;
        std::cout  << ConnectSocket <<":" << &ConnectSocket <<std::endl;
        int iResult;
        struct sockaddr_in from{};
        socklen_t fromlen = sizeof(from);
        do {
            //std::string recvbf;
            char recvbf[DEFAULT_BUFLEN];
            //iResult = recvOneTime(ConnectSocket, recvbf, DEFAULT_BUFLEN, from);
            //iResult = 1;
            iResult = recvfrom(ConnectSocket, recvbf, DEFAULT_BUFLEN, 0, (sockaddr *)(&from), &fromlen);
            std::cout << recvbf <<std::endl;
            char bufy[DEFAULT_BUFLEN] = "0 0";
            sendto(ConnectSocket, bufy, DEFAULT_BUFLEN, 0, (sockaddr *)(&from), fromlen);

            std::vector<std::string> vec = split(std::string(recvbf), " ");
            if ( iResult > 0 ) {
                if (toFile) {
                    file << "Bytes received: ";
                    file << iResult;
                    file << "\n";

                } else {
                    printf("Bytes received: %d\n", iResult);
                }
                std::cout << recvbf << std::endl;
            }
            else if ( iResult == 0 ) {
                printf("Connection closed\n");
                break;
            }
            else {
                printf("recv failed with error:\n");
                break;
            }

        } while( iResult > 0 );
        shutdown(ConnectSocket, 2);
        closesocket(ConnectSocket);
        return 0;
}



int main(int argc, char *argv[]) {
    WSAData wsaData {};
    SOCKET newConnectSocket, ConnectSocket = INVALID_SOCKET;
    struct addrinfo *result = nullptr,
                    hints{};
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;


    ZeroMemory(&hints, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    iResult = getaddrinfo("127.0.0.1", "27021", &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }
    pthread_mutex_init(&recv_send_mutex, nullptr);

    ConnectSocket = socket(result->ai_family, result->ai_socktype,
        result->ai_protocol);

    if (ConnectSocket == -1) {
        printf("socket failed with error\n");
        WSACleanup();
        return 1;
    }


    freeaddrinfo(result);

    struct sockaddr_in server{};
	server.sin_family      = AF_INET;            /* Internet Domain    */
   	server.sin_port        = htons(27021);               /* Server Port        */
   	server.sin_addr.s_addr = inet_addr("127.0.0.1");
   	int len = sizeof(server);


    std::cout<< "Do you want to display output? y/n\n" <<std::endl;
    std::string outputString;
    std::getline(std::cin, outputString);
    if (outputString == "y") {
        toFile = false;
    } else {
        file.open("log.txt");
        toFile = true;
    }
    std::string endString = "end";

    std::string connectstr = "hello";
    //
    std::string buf;
    char testBuf[DEFAULT_BUFLEN] = "0 hello";
    sendto(ConnectSocket, testBuf, recvbuflen, 0, (sockaddr *)(&server), len);
    recvfrom(ConnectSocket, testBuf, recvbuflen, 0, (sockaddr *)(&server), &len);
    std::cout<<testBuf<<std::endl;
    //std::cout << sendOneTime(ConnectSocket, testBuf, recvbuflen, server) << std::endl;
    struct sockaddr_in server1{};
    int len1 = sizeof(server1);
    recvfrom(ConnectSocket, testBuf, recvbuflen, 0, (sockaddr *)(&server1), &len1);
    std::cout << testBuf <<std::endl;
    char bufy[DEFAULT_BUFLEN] = "0 0";
    sendto(ConnectSocket, bufy, recvbuflen, 0, (sockaddr *)(&server1), len1);
    //if (recvOneTime(ConnectSocket, buf, DEFAULT_BUFLEN, server) <= 0) return 1;
    std::string recvString = std::string(testBuf);
    server.sin_port = htons(atoi(testBuf));

    struct addrinfo *resultnew = nullptr;

    iResult = getaddrinfo("127.0.0.1", testBuf, &hints, &resultnew);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    //shutdown(ConnectSocket, 2);
    //closesocket(ConnectSocket);
    newConnectSocket = socket(resultnew->ai_family, resultnew->ai_socktype,
                              resultnew->ai_protocol);

    if (newConnectSocket == -1) {
        printf("socket failed with error\n");
        WSACleanup();
        return 1;
    }
    freeaddrinfo(resultnew);

    struct sockaddr_in newserver{};
    newserver.sin_family      = AF_INET;            /* Internet Domain    */
    newserver.sin_port        = htons(atoi(testBuf));               /* Server Port        */
    newserver.sin_addr.s_addr = inet_addr("127.0.0.1");
    int newlen = sizeof(newserver);
    std::cout  << newConnectSocket <<":" << &newConnectSocket <<std::endl;

    pthread_t acceptThread;
    pthread_create(&acceptThread, nullptr, &readFunc, (void*) (&newConnectSocket));
    printf("Valid commands (Enter the command, not number):\n1) show\n2) getTest <number of test>\n3) getResult \n result of the last test\n4) register\n <press Enter> ---> <login> <password>\n");
    while(true) {

        std::string str;
        getline(std::cin, str);
        //"пидзык"

        //int Result = sendOneTime(newConnectSocket, const_cast<char *>(str.c_str()), recvbuflen, newserver);

        sendto(newConnectSocket, str.c_str(), recvbuflen, 0, (sockaddr *)(&newserver), newlen);
        //char newbuf[DEFAULT_BUFLEN];
        //recvfrom(ConnectSocket, newbuf, recvbuflen, 0, (sockaddr *)(&newserver), &newlen);
        //std::cout << newbuf << std::endl;
        //if (Result == 1)
        //break;
        if (str == endString) {
            break;
        }
    }
    shutdown(newConnectSocket, 2);
    closesocket(newConnectSocket);
    pthread_join(acceptThread, NULL);
    file.close();
    WSACleanup();
    return 0;
}