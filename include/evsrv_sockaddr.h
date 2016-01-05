#ifndef LIBEVSERVER_EVSRV_SOCKADDR_H
#define LIBEVSERVER_EVSRV_SOCKADDR_H

#include <sys/un.h>
#include <netinet/in.h>

struct evsrv_sockaddr {
    struct sockaddr_storage ss;
    socklen_t slen;
};

#endif //LIBEVSERVER_EVSRV_SOCKADDR_H
