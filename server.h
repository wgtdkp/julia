#ifndef _JULIA_SERVER_H_
#define _JULIA_SERVER_H_

#include <stdio.h>
#include <stdlib.h>


#define MAX_EVENT_NUM   (65536)
#define RECV_BUF_SIZE   (4 * 1024)
#define SEND_BUF_SIZE   (4 * 1024)
#define EVENTS_IN   (EPOLLIN | EPOLLET)
#define EVENTS_OUT  (EPOLLOUT | EPOLLET)

extern int doc_root_fd;

#endif
