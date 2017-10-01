#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <winsock2.h>
#include <stdint.h>


int main() {

    WSADATA wsaData;

    unsigned int t;
    t = WSAStartup(MAKEWORD(2,2), &wsaData);

    if (t != 0) {
        printf("WSAStartup failed: %ui\n", t);
        return 1;
    }

    int sockfd;
    uint16_t portno;
    int clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;

    /* First call to socket() function */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

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

    clilen = sizeof(cli_addr);
    int rec = recvfrom(sockfd, buffer, 256, 0, (struct sockaddr *)&cli_addr, &clilen);
    if (rec < 0) {
    	perror("ERROR");
    	exit(1);
    }

    struct hostent *hst;
        hst = gethostbyaddr((char *)&cli_addr.sin_addr, 4, AF_INET);
        printf("+%s [%s:%d] new DATAGRAM!\n", (hst) ? hst->h_name : "Unknown host", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        printf("C=>S:%s\n", &buffer[0]);
 
        sendto(sockfd, buffer, 256, 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
        close(sockfd);
        WSACleanup();

    return 0;
}