#include "server.h"

void backend_open_connection(location_t* loc)
{
    assert(loc->pass);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    EXIT_ON(fd == -1, "socket");

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(loc->port);
    int success = inet_pton(AF_INET, loc->host.data, &addr.sin_addr);
    EXIT_ON(success <= 0, "inet_pton");
    
    int err = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    EXIT_ON(err < 0, "connect");
    loc->fd = fd;
}

void backend_close_connection(location_t* loc)
{
    close(loc->fd);
}
