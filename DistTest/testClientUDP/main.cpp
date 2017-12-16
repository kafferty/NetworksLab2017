#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x501

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <pthread.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27021"

bool toFile = false;
std::ofstream file;



std::vector<std::string> split(std::string s, std::string delimiter){ //Скопировано из интернета
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

ssize_t recvOneTime(SOCKET &connectSocket, std::string &final, size_t recvbuflen, std::string packetNumber) {
    struct sockaddr_in server {};
    char buf[DEFAULT_BUFLEN];
    socklen_t len = sizeof(server);
    ssize_t size = recvfrom(connectSocket, buf, recvbuflen, 0, (sockaddr *)(&server), &len);
    std::vector<std::string> recvVec = split(std::string(buf), " ");
    if (recvVec[0] != packetNumber) return -2;
    for(int i = 1; i < recvVec.size(); i++) {
        final.append(recvVec[i]);
        if (i == recvVec.size() - 1) break;
        final.append(" ");
    }
    return size;
}

ssize_t sendOneTime(SOCKET  connectSocket, char * connectstr, sockaddr_in server, std::string packetNumber) {
    socklen_t len = sizeof(server);
    char connectString[DEFAULT_BUFLEN] = "";
    strncat(connectString, (packetNumber + " ").c_str(), (packetNumber + " ").size());
    strncat(connectString, connectstr, 512 - (packetNumber + " ").size());
    ssize_t iSend = sendto(connectSocket, connectString, DEFAULT_BUFLEN, 0, (sockaddr *)(&server), len);
    return iSend;
}

void readFunc(SOCKET connectSocket) {
    ssize_t iResult;
    struct sockaddr_in from{};
    socklen_t fromlen = sizeof(from);
    int counter = 0;
    do {
        std::string recvstr;
        char counterString[10];
        sprintf(counterString, "%d", counter);
        iResult = recvOneTime(connectSocket, recvstr, DEFAULT_BUFLEN, std::string(counterString));
        if ( iResult > 0 ) {
            if (toFile) {
                file << "Bytes received: ";
                file << iResult;
                file << "\n";

            } else {
                printf("Bytes received: %d\n", iResult);
            }
            if(recvstr == "FUNCTION END") {
                break;
            }
            else counter = counter + 1;
            std::cout << recvstr << std::endl;
        }
        else if ( iResult == 0 ) {
            printf("Connection closed\n");
            break;
        }
        else {
            printf("recv failed with error: %d \n", iResult);
            break;
        }

    } while( iResult > 0 );
}



int main(int argc, char *argv[]) {
    WSAData wsaData {};
    SOCKET connectSocket;
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

    connectSocket = socket(result->ai_family, result->ai_socktype,
                           result->ai_protocol);

    if (connectSocket == -1) {
        printf("socket failed with error\n");
        WSACleanup();
        return 1;
    }


    freeaddrinfo(result);

    struct sockaddr_in server{};
    server.sin_family      = AF_INET;
    server.sin_port        = htons(27021);
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

    std::cout << &connectSocket << std::endl;
    std:: cout<<("Valid commands (Enter the command, not number):\n1) show\n2) getTest <number of test>\n3) getResult \n result of the last test\n4) register\n <press Enter> ---> <login> <password>\n");
    while(true) {

        std::string str;
        getline(std::cin, str);

        sendOneTime(connectSocket, const_cast<char *> (str.c_str()), server, "0");
        readFunc(connectSocket);
        if (str == endString) {
            break;
        }
    }
    shutdown(connectSocket, 2);
    closesocket(connectSocket);
    file.close();
    WSACleanup();
    return 0;
}