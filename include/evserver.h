#ifndef LIBEVSERVER_EVSERVER_H
#define LIBEVSERVER_EVSERVER_H

#include <ev.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "platform.h"
#include "util.h"
#include "evsrv_sockaddr.h"

#ifndef EVSRV_USE_TCP_NO_DELAY
#  define EVSRV_USE_TCP_NO_DELAY 1
#endif

#ifndef EVSRV_DEFAULT_BUF_LEN
#  define EVSRV_DEFAULT_BUF_LEN 4096
#endif


typedef struct _evserver evserver;
typedef struct _evserver_info evserver_info;
typedef struct _evsrv evsrv;
typedef struct _evsrv_conn_info evsrv_conn_info;
typedef struct _evsrv_conn evsrv_conn;


typedef void        (* c_cb_started_t)(void*);
typedef evsrv*      (* c_cb_srv_create_t)(evserver*, size_t, evserver_info*);
typedef void        (* c_cb_srv_destroy_t)(evsrv*);

typedef evsrv_conn* (* c_cb_conn_create_t)(evsrv*, evsrv_conn_info*);
typedef void        (* c_cb_conn_ready_t)(evsrv_conn*);
typedef void        (* c_cb_conn_close_t)(evsrv_conn*, int err);
typedef void        (* c_cb_read_t)(evsrv_conn*, ssize_t);

typedef void        (* c_cb_evserver_graceful_stop_t)(evserver*);
typedef void        (* c_cb_evsrv_graceful_stop_t)(evsrv*);
typedef void        (* c_cb_graceful_close_t)(evsrv_conn*);

struct _evserver_info {
    char* host;
    char* port;

    c_cb_srv_create_t on_create;
    c_cb_srv_destroy_t on_destroy;
};

typedef enum {
    EVSERVER_IDLE,
    EVSERVER_LISTENING,
    EVSERVER_ACCEPTING,
    EVSERVER_GRACEFULLY_STOPPING,
    EVSERVER_STOPPED
} evserver_state_t;

struct _evserver {
    struct ev_loop* loop;
    evserver_state_t state;

    c_cb_started_t on_started;
    c_cb_evserver_graceful_stop_t on_graceful_stop;

    evsrv** srvs;
    size_t srvs_len;
    size_t stopped_srvs;
    int active_srvs;
};

void evserver_init(evserver* self, evserver_info* servers, size_t servers_count);
void evserver_clean(evserver* self);
void evserver_listen(evserver* self);
void evserver_accept(evserver* self);
void evserver_notify_fork_child(evserver* self);
void evserver_stop(evserver* self);
void evserver_graceful_stop(evserver* self, c_cb_evserver_graceful_stop_t cb);


typedef enum {
    EVSRV_IDLE,
    EVSRV_LISTENING,
    EVSRV_ACCEPTING,
    EVSRV_GRACEFULLY_STOPPING,
    EVSRV_STOPPED
} evsrv_state_t;

struct _evsrv {
    struct ev_loop* loop;
    evserver* server;
    size_t id;
    evsrv_state_t state;
    time_t now;

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

    int active_connections;
    evsrv_conn** connections;
    size_t connections_len;

    void* data;
};

void evsrv_init(evsrv* self, size_t id, const char* host, const char* port);
void evsrv_clean(evsrv* self);
int evsrv_listen(evsrv* self);
int evsrv_accept(evsrv* self);
void evsrv_notify_fork_child(evsrv* self);
void evsrv_stop(evsrv* self);
void evsrv_graceful_stop(evsrv* self, c_cb_evsrv_graceful_stop_t cb);


struct _evsrv_conn_info {
    int sock;
    struct evsrv_sockaddr addr;
};

struct _evsrv_conn {
    evsrv* srv;
    evsrv_conn_info* info;
    enum {
        EVSRV_CONN_CREATED,
        EVSRV_CONN_ACTIVE,
        EVSRV_CONN_SHUTDOWN,
        EVSRV_CONN_CLOSING,
        EVSRV_CONN_CLOSING_FORCE,
        EVSRV_CONN_STOPPED,
    } state;

    ev_io rw;
    ev_timer trw;

    ev_io ww;
    ev_timer tww;

    char* rbuf;
    uint32_t ruse;
    uint32_t rlen;

    struct iovec* wbuf;
    uint32_t wuse;
    uint32_t wlen;
    int wnow;

    c_cb_read_t on_read;
    c_cb_graceful_close_t on_graceful_close;

    void* data;
};

void evsrv_conn_init(evsrv_conn* self, evsrv* srv, evsrv_conn_info* info);
void evsrv_conn_start(evsrv_conn* self);
void evsrv_conn_stop(evsrv_conn* self);
void evsrv_conn_clean(evsrv_conn* self);
void evsrv_conn_shutdown(evsrv_conn* self, int how);
void evsrv_conn_close(evsrv_conn* self, int err);

void evsrv_write(evsrv_conn* conn, const char* buf, size_t len);


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

#endif //LIBEVSERVER_EVSERVER_H
