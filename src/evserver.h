#ifndef LIBEVSERVER_EVSERVER_H
#define LIBEVSERVER_EVSERVER_H

#include <ev.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include "util.h"
#include "platform.h"

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

struct _evserver_info {
    char* host;
    char* port;

    c_cb_srv_create_t on_create;
    c_cb_srv_destroy_t on_destroy;
};

struct _evserver {
    struct ev_loop* loop;
    c_cb_started_t on_started;

    evsrv** srvs;
    size_t srvs_len;
};

void evserver_init(evserver* self, evserver_info* servers, size_t servers_count);
void evserver_clean(evserver* self);
void evserver_listen(evserver* self);
void evserver_accept(evserver* self);
void evserver_notify_fork_child(evserver* self);
void evserver_run(evserver* self);
void evserver_stop(evserver* self);


typedef enum {
    EVSRV_IDLE,
    EVSRV_LISTENING,
    EVSRV_ACCEPTING,
    EVSRV_STOPPED
} evsrv_state_t;

struct _evsrv {
    struct ev_loop* loop;
    size_t id;
    evsrv_state_t state;

    int sock;
    int backlog;

    ev_io accept_rw;
    double read_timeout;
    double write_timeout;

    c_cb_srv_destroy_t on_destroy;
    c_cb_started_t on_started;
    c_cb_conn_create_t on_conn_create;
    c_cb_conn_ready_t on_conn_ready;
    c_cb_conn_close_t on_conn_close;
    c_cb_read_t on_read;

    const char* host;
    const char* port;
    struct sockaddr* sock_addr;
    socklen_t sock_addr_len;

    time_t now;
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
void evsrv_run(evsrv* self);
void evsrv_stop(evsrv* self);



struct _evsrv_conn_info {
    int sock;

};

struct _evsrv_conn {
    enum {
        EVSRV_CONN_CREATED,
        EVSRV_CONN_ACTIVE,
        EVSRV_CONN_SHUTDOWN,
        EVSRV_CONN_CLOSING,
        EVSRV_CONN_CLOSING_FORCE,
        EVSRV_CONN_STOPPED,
        EVSRV_CONN_CLOSED
    } state;
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



void evsrv_conn_init(evsrv_conn* self, evsrv* srv, evsrv_conn_info* info);
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
