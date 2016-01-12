#ifndef LIBEVSERVER_EVSRV_H
#define LIBEVSERVER_EVSRV_H

#include <stddef.h>
#include <stdint.h>
#include <ev.h>

#include "common.h"
#include "evsrv_conn.h"

typedef void        (* c_cb_started_t)(void*);
typedef void        (* c_cb_srv_destroy_t)(evsrv*);

typedef evsrv_conn* (* c_cb_conn_create_t)(evsrv*, struct evsrv_conn_info*);
typedef void        (* c_cb_conn_ready_t)(evsrv_conn*);
typedef void        (* c_cb_conn_close_t)(evsrv_conn*, int err);
typedef void        (* c_cb_evsrv_graceful_stop_t)(evsrv*);

enum evsrv_state {
    EVSRV_IDLE,
    EVSRV_LISTENING,
    EVSRV_ACCEPTING,
    EVSRV_GRACEFULLY_STOPPING,
    EVSRV_STOPPED
};

struct _evsrv_manager;
struct _evsrv {
    struct ev_loop* loop;
    struct _evsrv_manager* manager;
    size_t id;
    enum evsrv_state state;

    enum evsrv_proto proto;
    const char* host;
    const char* port;
    struct evsrv_sockaddr sockaddr;

    int sock;
    int backlog;
    double read_timeout;

    double write_timeout;
    ev_io accept_rw;

    c_cb_srv_destroy_t on_destroy;
    c_cb_started_t on_started;
    c_cb_conn_create_t on_conn_create;
    c_cb_conn_ready_t on_conn_ready;
    c_cb_conn_close_t on_conn_close;
    c_cb_read_t on_read;

    c_cb_evsrv_graceful_stop_t on_graceful_stop;

    int32_t active_connections;
    evsrv_conn** connections;
    size_t connections_len;

    void* data;
};

typedef struct _evsrv evsrv;

void evsrv_init(evsrv* self, enum evsrv_proto proto, const char* host, const char* port);
void evsrv_clean(evsrv* self);
int evsrv_listen(evsrv* self);
int evsrv_accept(evsrv* self);
void evsrv_stop(evsrv* self);
void evsrv_graceful_stop(evsrv* self, c_cb_evsrv_graceful_stop_t cb);

#endif //LIBEVSERVER_EVSRV_H
