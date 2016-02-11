#ifndef LIBEVSERVER_EVSRV_H
#define LIBEVSERVER_EVSRV_H

#include <stddef.h>
#include <stdint.h>
#include <ev.h>

#include "common.h"
#include "evsrv_conn.h"

EV_CPP(extern "C" {)

typedef void        (* evsrv_on_started_cb)(evsrv*);
typedef void        (* evsrv_on_destroy_cb)(evsrv*);

typedef evsrv_conn* (* evsrv_on_conn_create_cb)(evsrv*, struct evsrv_conn_info*);
typedef void        (* evsrv_on_conn_ready_cb)(evsrv_conn*);
typedef void        (* evsrv_on_conn_destroy_cb)(evsrv_conn*, int err);
typedef void        (* evsrv_on_graceful_stop_cb)(evsrv*);

enum evsrv_state {
    EVSRV_IDLE,
    EVSRV_BOUND,
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

    evsrv_on_destroy_cb on_destroy;
    evsrv_on_started_cb on_started;
    evsrv_on_conn_create_cb on_conn_create;
    evsrv_on_conn_ready_cb on_conn_ready;
    evsrv_on_conn_destroy_cb on_conn_destroy;
    evsrv_on_read_cb on_read;

    evsrv_on_graceful_stop_cb on_graceful_stop;

    int32_t active_connections;
    evsrv_conn** connections;
    size_t connections_len;

    void* data;
};

typedef struct _evsrv evsrv;

void evsrv_init(struct ev_loop* loop, evsrv* self, const char* host, const char* port);
void evsrv_destroy(evsrv* self);
int evsrv_bind(evsrv* self);
int evsrv_listen(evsrv* self);
int evsrv_accept(evsrv* self);
void evsrv_stop(evsrv* self);
void evsrv_graceful_stop(evsrv* self, evsrv_on_graceful_stop_cb cb);


#define evsrv_set_on_started(srv, on_started_cb) do { \
    (srv)->on_started = (evsrv_on_started_cb) (on_started_cb); \
} while (0)


#define evsrv_set_on_read(srv, on_read_cb) do { \
    (srv)->on_read = (evsrv_on_read_cb) (on_read_cb); \
} while (0)


#define evsrv_set_on_conn_ready(srv, on_conn_ready_cb) do { \
    (srv)->on_conn_ready = (evsrv_on_conn_ready_cb) (on_conn_ready_cb); \
} while (0)


#define evsrv_set_on_conn(srv, on_create_cb, on_destroy_cb) do { \
    (srv)->on_conn_create = (evsrv_on_conn_create_cb) (on_create_cb); \
    (srv)->on_conn_destroy = (evsrv_on_conn_destroy_cb) (on_destroy_cb); \
} while (0)

EV_CPP(})

#endif //LIBEVSERVER_EVSRV_H
