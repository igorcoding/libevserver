#ifndef LIBEVSERVER_EVSERVER_H
#define LIBEVSERVER_EVSERVER_H

#include <ev.h>
#include <stdint.h>

#include "util.h"

#ifndef EVSRV_DEFAULT_BUF_LEN
#  define EVSRV_DEFAULT_BUF_LEN 4096
#endif

typedef enum {
    EVSRV_IDLE,
    EVSRV_LISTENING,
    EVSRV_ACCEPTING,
    EVSRV_STOPPED
} state_t;

typedef struct {
    int sock;

} evsrv_conn_info;

struct _evsrv_conn;

typedef void (*c_cb_started_t)(void*);
typedef struct _evsrv_conn* (* c_cb_conn_create_t)(void*, evsrv_conn_info*);
typedef void (* c_cb_conn_ready_t)(struct _evsrv_conn*);
typedef void (* c_cb_conn_close_t)(struct _evsrv_conn*, int err);
typedef void (*c_cb_read_t)(struct _evsrv_conn*, ssize_t);

typedef struct {
    struct ev_loop* loop;
    state_t state;

    int sock;
    int backlog;

    ev_io accept_rw;
    double read_timeout;
    double write_timeout;

    c_cb_started_t on_started;
    c_cb_conn_create_t on_conn_create;
    c_cb_conn_ready_t on_conn_ready;
    c_cb_conn_close_t on_conn_close;
    c_cb_read_t on_read;

    char* host;
    short unsigned int port;
    uint32_t rlen;

    time_t now;
    int active_connections;

    struct _evsrv_conn** connections;
    size_t connections_len;
} evsrv;

void evsrv_init(evsrv* self);
void evsrv_clean(evsrv* self);
int evsrv_listen(evsrv* self);
int evsrv_accept(evsrv* self);
void evsrv_run(evsrv* self);




struct _evsrv_conn {
    evsrv* srv;
    evsrv_conn_info* info;

    ev_io rw;
    ev_timer trw;

    ev_io ww;
    ev_timer tww;

    struct iovec* wbuf;
    uint32_t wuse;
    uint32_t wlen;
    int wnow;

    char* rbuf;
    uint32_t ruse;
    uint32_t rlen;

    c_cb_read_t on_read;
};

typedef struct _evsrv_conn evsrv_conn;

void evsrv_conn_init(evsrv_conn* self, evsrv* srv, evsrv_conn_info* info);
void evsrv_conn_stop(evsrv_conn* self);
void evsrv_conn_clean(evsrv_conn* self);
void evsrv_conn_close(evsrv_conn* self, int err);

void evsrv_write(evsrv_conn* conn, const char* buf, size_t len);


#endif //LIBEVSERVER_EVSERVER_H
