#ifndef LIBEVSERVER_SOCKADDR_UNION_H
#define LIBEVSERVER_SOCKADDR_UNION_H

#include <sys/un.h>
#include <netinet/in.h>

struct evsrv_sockaddr {
    struct sockaddr_storage ss;
    socklen_t slen;
};

#endif //LIBEVSERVER_SOCKADDR_UNION_H
