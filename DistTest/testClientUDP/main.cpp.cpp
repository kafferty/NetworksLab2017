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
void * readFunc(void* pArguments) {
        SOCKET ConnectSocket = *(SOCKET*) pArguments;
        int iResult;
        struct sockaddr_in from{};
        socklen_t fromlen = sizeof(from);
        do {
            char recvbf[2 * DEFAULT_BUFLEN];
            int recvbuflen = 2 * DEFAULT_BUFLEN;
            iResult = recvfrom(ConnectSocket, recvbf, recvbuflen, 0, (sockaddr *) (&from), &fromlen);
            std::vector<std::string> vec = split(std::string(recvbf), " ");
       		sendto(ConnectSocket, const_cast<char *>(("0 " + vec[0]).c_str()), DEFAULT_BUFLEN, 0, (sockaddr *) (&from), fromlen);
        	std::string recvbuf;
        	for (int i = 1; i < vec.size(); i++)
            	recvbuf.append(vec[i]);
            if ( iResult > 0 ) {
                if (toFile) {
                    file << "Bytes received: ";
                    file << iResult;
                    file << "\n";

                } else {
                    printf("Bytes received: %d\n", iResult);
                }
                std::cout << recvbuf << std::endl;
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
int main(int argc, char *argv[])
{
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

    std::string connectstr = "0 hello";
    sendto(ConnectSocket, connectstr.c_str(), 2 * recvbuflen, 0, (sockaddr *)(&server), len);
    char buf[2*DEFAULT_BUFLEN];
    recvfrom(ConnectSocket, buf, 2 * recvbuflen, 0, (sockaddr *)(&server), &len);
    std::cout<< std::string(buf) << std::endl;
    //std::vector<std::string> vec = split(std::string(buf), " ");
    recvfrom(ConnectSocket, buf, 2 * recvbuflen, 0, (sockaddr *)(&server), &len);
    std::vector<std::string> vec = split(std::string(buf), " ");
    std::string sendback = vec[0] + " 0";
    sendto(ConnectSocket, sendback.c_str(), 2 * recvbuflen, 0, (sockaddr *)(&server), len);

    server.sin_port = htons(atoi(vec[1].c_str()));

    iResult = getaddrinfo(nullptr, const_cast<char *>(vec[1].c_str()), &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    shutdown(ConnectSocket, 2);
    closesocket(ConnectSocket);
    newConnectSocket = socket(result->ai_family, result->ai_socktype,
        result->ai_protocol);

    if (newConnectSocket == -1) {
        printf("socket failed with error\n");
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);

    pthread_t acceptThread;
    pthread_create(&acceptThread, nullptr, &readFunc, (void*) &newConnectSocket);
    printf("Valid commands (Enter the command, not number):\n1) show\n2) getTest <number of test>\n3) getResult \n result of the last test\n4) register\n <press Enter> ---> <login> <password>\n");
    while(true) {

        std::string str;
        getline(std::cin, str);

        int Result = sendto(newConnectSocket, str.c_str(), 2 * recvbuflen, 0, (sockaddr *)(&server), len);
        if (Result == -1)
            break;
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