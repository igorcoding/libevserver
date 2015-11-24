#ifndef LIBEVSERVER_PLATFORM_H
#define LIBEVSERVER_PLATFORM_H

#if defined(__unix__)
#  include <sys/socket.h>
#elif defined(_WIN32)
#  include <Winsock2.h>
#endif

#if defined(__unix__)
#  define EVSRV_SHUT_RD SHUT_RD
#  define EVSRV_SHUT_WR SHUT_WR
#  define EVSRV_SHUT_RDWR SHUT_RDWR
#elif defined(_WIN32)
#  define EVSRV_SHUT_RD SD_RECEIVE
#  define EVSRV_SHUT_WR SD_SEND
#  define EVSRV_SHUT_RDWR SD_BOTH
#endif

#endif //LIBEVSERVER_PLATFORM_H
