#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <string.h>
#include "pthread.h"

int readn(int sockfd, char *buf, int n){
    int k;
    int off = 0;
    for(int i = 0; i < n; ++i){
        k = read(sockfd, buf + off, 1);
        off += 1;
        if (k < 0){
            printf("Error. reading from socket \n");
            exit(1);
        }
    }
    return off;
}

void* formulti (void* temp) {
	int n = *((int*)temp);
	char buffer[256];
	char *p = buffer;
		bzero(buffer, 256);
		int k = readn (n, p, 255);
		if (k > 0) {
			printf("%s \n", buffer);
			write(n, "I have got your message", 23);
		}
    shutdown(n, 2);
    close(n);
}



int main() {
    int sockfd, newsockfd;
    uint16_t portno;
    unsigned int clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    ssize_t n;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 5001;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    while (1) {
        listen(sockfd, 5);
        clilen = sizeof(cli_addr);

        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);


        if (newsockfd < 0) {
            perror("ERROR on accept");
            exit(1);
        }


        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_create(&tid, &attr, formulti, &newsockfd);
        pthread_detach(tid);
    }

    return 0;
}