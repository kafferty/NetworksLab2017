#include <stdio.h> 
#include <stdlib.h> 
#include <winsock2.h> 
#include <stdint.h> 
#include <string.h> 

int readn(int sockfd, char *buf, int n){
    int k;
    int off = 0;
    for(int i = 0; i < n; ++i){
        k = recv(sockfd, buf + off, 1, 0);
        off += 1;
        if (k < 0){
            printf("Error. reading from socket \n");
            exit(1);
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {

    WSADATA wsaData;

    unsigned int t;
    t = WSAStartup(MAKEWORD(2,2), &wsaData);

    if (t != 0) {
        printf("WSAStartup failed: %ui\n", t);
        return 1;
    }

    int sockfd, newsockfd;
    uint16_t portno;
    unsigned int clilen;
    char buffer[256];
    char *p = buffer;
    struct sockaddr_in serv_addr, cli_addr;
    ssize_t n;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    portno = 5001;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }


    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    shutdown(sockfd, 2);
    closesocket(sockfd);
    
    if (newsockfd < 0) {
        perror("ERROR on accept");
        exit(1);
    }

    memset(buffer, 0, 256);

    readn(newsockfd, p, 255);

    printf("Here is the message: %s\n", buffer);

    n = send(newsockfd, "I got your message", 18, 0); 

    if (n < 0) {
        perror("ERROR writing to socket");
        exit(1);
    }


    t = shutdown(newsockfd, SD_BOTH);
    if (n == SOCKET_ERROR) {
        printf("shutdown failed: %d\n", WSAGetLastError());
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }
    closesocket(newsockfd);
    WSACleanup();
    return 0;
}