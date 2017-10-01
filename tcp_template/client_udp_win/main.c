#include <stdio.h>
#include <winsock2.h>
#include <stdint.h>


int main(int argc, char *argv[]) {

    WSADATA wsaData;

    unsigned int t;
    t = WSAStartup(MAKEWORD(2,2), &wsaData);

    if (t != 0) {
        printf("WSAStartup failed: %ui\n", t);
        return 1;
    }

    int sockfd, n;
    uint16_t portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];

    if (argc < 3) {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }

    portno = (uint16_t) atoi(argv[2]);

    /* Create a socket point */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    server = gethostbyname(argv[1]);

    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *) &serv_addr.sin_addr.s_addr, server->h_addr, (size_t) server->h_length);
    serv_addr.sin_port = htons(portno);

    printf("Please enter the message: ");
    memset(buffer, 0, 256);
    fgets(buffer, 255, stdin);

    /* Send message to the server */
    n = sendto(sockfd, buffer, 256, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    if (n < 0) {
        perror("ERROR writing to socket");
        exit(1);
    }

    /* Now read server response */
    memset(buffer, 0, 256);

    struct sockaddr_in serv_two;
    int serv_two_size = sizeof(serv_two);

    int rec = recvfrom(sockfd, buffer, 256, 0, (struct sockaddr *)&serv_two, &serv_two_size);
    if (rec < 0) {
        perror("ERROR");
        exit(1);
    }

    printf("%s\n", buffer);

    closesocket(sockfd);
    WSACleanup();

    return 0;
}