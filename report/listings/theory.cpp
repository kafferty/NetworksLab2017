int socket(int domain, int type, int protocol);
int connect(int s, const struct sockaddr* serv_addr, int addr_len);
int send(int s, const void *msg, size_t len, int flags);
int recv(int s, void *msg, size_t len, int flags);