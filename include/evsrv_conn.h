#ifndef LIBEVSERVER_EVSRV_CONN_H
#define LIBEVSERVER_EVSRV_CONN_H

#include <ev.h>
#include <stdint.h>
#include <stdbool.h>

#include "common.h"

typedef struct _evsrv_conn evsrv_conn;
typedef struct _evsrv evsrv;

typedef void (* c_cb_read_t)(evsrv_conn*, ssize_t);
typedef bool (* c_cb_graceful_close_t)(evsrv_conn*);

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

    int8_t* rbuf;
    size_t ruse;
    size_t rlen;

    struct iovec* wbuf;
    size_t wuse;
    size_t wlen;
    bool wnow;

    c_cb_read_t on_read;
    c_cb_graceful_close_t on_graceful_close;

    void* data;
};

void evsrv_conn_init(evsrv_conn* self, evsrv* srv, struct evsrv_conn_info* info);
void evsrv_conn_start(evsrv_conn* self);
void evsrv_conn_stop(evsrv_conn* self);
void evsrv_conn_clean(evsrv_conn* self);
void evsrv_conn_shutdown(evsrv_conn* self, int how);
void evsrv_conn_close(evsrv_conn* self, int err);

void evsrv_conn_write(evsrv_conn* conn, const void* buffer, size_t len);

#endif //LIBEVSERVER_EVSRV_CONN_H
