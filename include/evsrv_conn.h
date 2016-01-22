#ifndef LIBEVSERVER_EVSRV_CONN_H
#define LIBEVSERVER_EVSRV_CONN_H

#include <ev.h>
#include <stdint.h>
#include <stdbool.h>

#include "common.h"

EV_CPP(extern "C" {)

typedef struct _evsrv_conn evsrv_conn;
typedef struct _evsrv evsrv;

typedef void (* evsrv_on_read_cb)(evsrv_conn*, ssize_t);
typedef bool (* evsrv_conn_on_graceful_close_cb)(evsrv_conn*);

struct evsrv_conn_info {
    struct evsrv_sockaddr addr;
    int sock;
};

enum evsrv_conn_state {
    EVSRV_CONN_CREATED,
    EVSRV_CONN_ACTIVE,
    EVSRV_CONN_SHUTDOWN,
    EVSRV_CONN_CLOSING,
    EVSRV_CONN_PENDING_CLOSE,
    EVSRV_CONN_STOPPED,
};

struct _evsrv_conn {
    evsrv* srv;
    struct evsrv_conn_info* info;
    enum evsrv_conn_state state;

    ev_io rw;
    ev_timer trw;

    ev_io ww;
    ev_timer tww;

    char* rbuf;
    size_t ruse;
    size_t rlen;

    struct iovec* wbuf;
    size_t wuse;
    size_t wlen;
    bool wnow;

    evsrv_on_read_cb on_read;
    evsrv_conn_on_graceful_close_cb on_graceful_close;

    void* data;
};

void evsrv_conn_init(evsrv_conn* self, evsrv* srv, struct evsrv_conn_info* info);
void evsrv_conn_start(evsrv_conn* self);
void evsrv_conn_stop(evsrv_conn* self);
void evsrv_conn_clean(evsrv_conn* self);
void evsrv_conn_shutdown(evsrv_conn* self, int how);
void evsrv_conn_close(evsrv_conn* self, int err);

void evsrv_conn_write(evsrv_conn* conn, const void* buffer, size_t len);


#define evsrv_conn_set_rbuf(conn, buf, len) do { \
    (conn)->rbuf = (buf); \
    (conn)->rlen = (len); \
} while (0)


#define evsrv_conn_set_on_read(conn, on_read_cb) do { \
    (conn)->on_read = (evsrv_on_read_cb) (on_read_cb); \
} while (0)


#define evsrv_conn_set_on_graceful_close(conn, on_graceful_close_cb) do { \
    (conn)->on_graceful_close = (evsrv_conn_on_graceful_close_cb) (on_graceful_close_cb); \
} while (0)

EV_CPP(})

#endif //LIBEVSERVER_EVSRV_CONN_H
