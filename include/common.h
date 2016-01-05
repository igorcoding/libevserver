#ifndef LIBEVSERVER_COMMON_H
#define LIBEVSERVER_COMMON_H

#include <sys/socket.h>

enum evsrv_proto {
    EVSRV_PROTO_TCP,
    EVSRV_PROTO_UDP
};

#ifndef IOV_MAX
#  ifdef UIO_MAXIOV
#    define IOV_MAX UIO_MAXIOV
#  else
#    define IOV_MAX 1024
#  endif
#endif

#ifndef EVSRV_USE_TCP_NO_DELAY
#  define EVSRV_USE_TCP_NO_DELAY 1
#endif

#ifndef EVSRV_DEFAULT_BUF_LEN
#  define EVSRV_DEFAULT_BUF_LEN 4096
#endif

#ifndef EVSRV_SHUT_RD
#  define EVSRV_SHUT_RD SHUT_RD
#  define EVSRV_SHUT_WR SHUT_WR
#  define EVSRV_SHUT_RDWR SHUT_RDWR
#endif

#define evsrv_stop_timer(loop, ev) do { \
    if (ev_is_active(ev)) { \
        ev_timer_stop(loop, ev); \
    } \
} while (0)

#define evsrv_stop_io(loop, ev) do { \
    if (ev_is_active(ev)) { \
        ev_io_stop(loop, ev); \
    } \
} while (0)

int evsrv_socket_set_nonblock(int fd);

#endif //LIBEVSERVER_COMMON_H
