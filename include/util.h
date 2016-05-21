#ifndef LIBEVSERVER_UTIL_H
#define LIBEVSERVER_UTIL_H


#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifndef cwarn
#define cwarn(fmt, ...) do{ \
        fprintf(stderr, "[WARN] %0.6f %s:%d: ", ev_now(EV_DEFAULT), __FILE__, __LINE__); \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
        if (fmt[strlen(fmt) - 1] != 0x0a) { fprintf(stderr, "\n"); } \
    } while(0)
#endif

#ifndef cdebug
/*#define cdebug(fmt, ...) do{ \
        fprintf(stderr, "[DEBU] %0.6f %s:%d: ", ev_now(EV_DEFAULT), __FILE__, __LINE__); \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
        if (fmt[strlen(fmt) - 1] != 0x0a) { fprintf(stderr, "\n"); } \
    } while(0)*/
#define cdebug(fmt, ...) do{} while(0)
#endif

#ifndef cerror
#define cerror(fmt, ...) do{ \
        fprintf(stderr, "[ERR] %0.6f %s:%d: ", ev_now(EV_DEFAULT), __FILE__, __LINE__); \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
        fprintf(stderr, " [%d: %s]", errno, strerror(errno)); \
        if (fmt[strlen(fmt) - 1] != 0x0a) { fprintf(stderr, "\n"); } \
    } while(0)
#endif

#ifndef warn
#  define warn cwarn
#endif

#ifndef likely
#  define likely(x) __builtin_expect((x),1)
#  define unlikely(x) __builtin_expect((x),0)
#endif

#define memdup(a,b) memcpy(malloc(b),a,b)

//#define SELFby(ptr,type,xx) (type*) ( (char*) ptr - (ptrdiff_t) &((type*) 0)-> xx );
#define SELFby(ptr, type, xx) (type*) ( (char *) (ptr) - offsetof(type, xx))

#endif //LIBEVSERVER_UTIL_H
