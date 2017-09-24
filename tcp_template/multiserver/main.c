#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <stdint.h>
#include <string.h>
#include "pthread.h"

void* formulti (void* temp) {
	int n = *((int*)temp);
	char buffer[256];
	char *p = buffer;
	while (1) {
		memset(buffer, 0, 256);
		int k = readn (n, p, 255);
		if (k > 0) {
			printf("%s \n", buffer);
			send(n, "I have got your message", 23, 0);
		}
	}
}

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
    return off;
}

int main(int argc, char *argv[]) {
    int sockfd, newsockfd;
    uint16_t portno;
    unsigned int clilen;
    char buffer[256];
    char *p = buffer;
    struct sockaddr_in serv_addr, cli_addr;
    ssize_t n;

    /* First call to socket() function */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    /* Initialize socket structure */
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    portno = 5001;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    /* Now bind the host address using bind() call.*/
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    /* Now start listening for the clients, here process will
       * go in sleep mode and will wait for the incoming connection
    */
    while (1) {
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    /* Accept actual connection from the client */
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    shutdown(sockfd, 2);
    closesocket(sockfd);
    
    if (newsockfd < 0) {
        perror("ERROR on accept");
        exit(1);
    }

    /* If connection is established then start communicating */
    //bzero(buffer, 256);

    /*n = readn(newsockfd, p, 255);

    printf("Here is the message: %s\n", buffer);


    n = write(newsockfd, "I got your message", 18); // send on Windows

    if (n < 0) {
        perror("ERROR writing to socket");
        exit(1);
    } */    
	pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_create(&tid, &attr, formulti, &newsockfd);
	pthread_detach(tid);
}
    shutdown(newsockfd, 2);
    closesocket(newsockfd);

    return 0;
}