#include "common.h"
#include <fcntl.h>

int evsrv_socket_set_nonblock(int fd) {
    int flags = 0;
#ifdef O_NONBLOCK
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}