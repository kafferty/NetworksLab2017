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

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27021"



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
unsigned int __stdcall readFunc(void* pArguments) {
        SOCKET ConnectSocket = *(SOCKET*) pArguments;
        int iResult;
        do {
            char recvbuf[DEFAULT_BUFLEN];
            int recvbuflen = DEFAULT_BUFLEN;
            iResult = readn(ConnectSocket, recvbuf, recvbuflen);
            if ( iResult > 0 ) {
                printf("Bytes received: %d\n", iResult);
                printf("Received data: \n%s\n", recvbuf);
            }
            else if ( iResult == 0 ) {
                printf("Connection closed\n");
                break;
            }
            else {
                printf("recv failed with error: %d\n", WSAGetLastError());
                break;
            }

        } while( iResult > 0 );
        shutdown(ConnectSocket, 2);
        closesocket(ConnectSocket);
        return 0;
}
int __cdecl main(int argc, char **argv)
{
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL,
                    *ptr = NULL,
                    hints;
    char sendbuf[DEFAULT_BUFLEN] = "Hello Server!!!";
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;

    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;


    iResult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

   for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {

        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);

        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %d\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
       break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }


    std::string endString = "end";
    SOCKET *nListenSocket = new SOCKET;
    *nListenSocket = ConnectSocket;
    unsigned acceptThreadId;
    HANDLE acceptThreadHandler = (HANDLE)_beginthreadex(NULL, 0, &readFunc, (void*) nListenSocket, 0, &acceptThreadId);
    while(true) {

        std::string str;
        getline(std::cin, str);

        int Result = send( ConnectSocket, str.c_str(), recvbuflen, 0 );
        if (Result == SOCKET_ERROR)
            break;
        if (str == endString) {
            break;
        }
    }
    shutdown(ConnectSocket, 2);
    closesocket(ConnectSocket);
    WaitForSingleObject(acceptThreadHandler, INFINITE );
    CloseHandle(acceptThreadHandler);
    WSACleanup();

    return 0;
}
