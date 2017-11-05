#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <strings.h>

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27021"

bool toFile = false;
std::ofstream file;

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
void * readFunc(void* pArguments) {
        int ConnectSocket = *(int*) pArguments;
        int iResult;
        do {
            char recvbuf[DEFAULT_BUFLEN];
            int recvbuflen = DEFAULT_BUFLEN;
            iResult = readn(ConnectSocket, recvbuf, recvbuflen);
            if ( iResult > 0 ) {
                if (toFile) {
                    file << "Bytes received: ";
                    file << iResult;
                    file << "\n";

                } else {
                    printf("Bytes received: %d\n", iResult);
                }
                printf("Received data: \n%s\n", recvbuf);
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
        close(ConnectSocket);
        return 0;
}
int main(int argc, char *argv[])
{
    int ConnectSocket = -1;
    struct addrinfo *result = NULL,
                    *ptr = NULL,
                    hints;
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;


    bzero( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;



    iResult = getaddrinfo(argv[1], argv[2], &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        return 1;
    }

   for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) { // пытаемся подключиться к сокету сервера

        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);

        if (ConnectSocket == -1) {
            printf("socket failed with error\n");
            return 1;
        }

        iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == -1) {
            close(ConnectSocket);
            ConnectSocket = -1;
            continue;
        }
       break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == -1) {
        printf("Unable to connect to server!\n");
        return 1;
    }

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
    pthread_t acceptThread;
    pthread_create(&acceptThread, NULL, &readFunc, (void*) &ConnectSocket);
    printf("Valid commands (Enter the command, not number):\n1) show\n2) getTest <number of test>\n3) getResult \n result of the last test\n4) register\n <press Enter> ---> <login> <password>\n");
    while(true) {

        std::string str;
        getline(std::cin, str);

        int Result = send( ConnectSocket, str.c_str(), recvbuflen, 0 );
        if (Result == -1)
            break;
        if (str == endString) {
            break;
        }
    }
    shutdown(ConnectSocket, 2);
    close(ConnectSocket);
    pthread_join(acceptThread, NULL);
    file.close();
    return 0;
}

