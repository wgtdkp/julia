#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

int open_client(const char* hostname, unsigned short port)
{
    struct hostent* host;
    struct sockaddr_in server_addr = {0};
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0)
        return -1;

    if (0 != inet_aton(hostname, &server_addr) != 0) {
        host = gethostbyaddr((const char*)&server_addr, 
            sizeof(server_addr), AF_INET);
    } else {
        host = gethostbyname(hostname);
    }

    if (NULL == host)
        return -2;

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = 
        ((struct in_addr*)host->h_addr_list[0])->s_addr;
    server_addr.sin_port = htons(port);
    
    if (0 > connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)))
        return -1;
    return client_sock;
}

int main(int args, char* argv[])
{
    if (3 != args) {
        printf("usage: %s <host> <port>\n", argv[0]);
        exit(-1);
    }
    const char* hostname = argv[1];
    unsigned short port = atoi(argv[2]);
    //const char* filename = argv[3];
    int client_sock = open_client(hostname, port);
    if (client_sock < 0) {
        fprintf(stderr, "connect to %s: %d failed\n", hostname, port);
        exit(-1);
    }
    
    
    char sendbuf[1024] = {0};
    char recvbuf[1024] = {0};
    //fgets(sendbuf, 1024, stdin);
    sprintf(sendbuf, "GET / HTTP/1.1 \r\n"
                     "Host: localhost \r\n"
                     "\r\n");
    write(client_sock, sendbuf, sizeof(sendbuf));
    printf("%s", sendbuf);
    read(client_sock, recvbuf, sizeof(recvbuf));
    //if ('#' == recvbuf[0]) {
    //    printf("session end\n");
    //    exit(0);
    //}
    printf("%s: %s", hostname, recvbuf);
    

    return 0;
}
