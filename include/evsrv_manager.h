#ifndef LIBEVSERVER_EVSERVER_H
#define LIBEVSERVER_EVSERVER_H

#include <ev.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include "evsrv.h"

EV_CPP(extern "C" {)

typedef struct _evsrv_manager evsrv_manager;
typedef struct _evsrv_info evsrv_info;

typedef void   (* evsrv_manager_on_started_cb)(evsrv_manager*);
typedef evsrv* (* evsrv_manger_on_create_t)(evsrv_manager*, size_t, evsrv_info*);
typedef void   (* evsrv_manager_on_graceful_stop_cb)(evsrv_manager*);

struct _evsrv_info {
    // enum evsrv_proto proto;
    char* host;
    char* port;

    evsrv_manger_on_create_t on_create;
    evsrv_on_destroy_cb on_destroy;
};

enum evsrv_manager_state {
    EVSRV_MANAGER_IDLE,
    EVSRV_MANAGER_BOUND,
    EVSRV_MANAGER_LISTENING,
    EVSRV_MANAGER_ACCEPTING,
    EVSRV_MANAGER_GRACEFULLY_STOPPING,
    EVSRV_MANAGER_STOPPED
};

struct _evsrv_manager {
    struct ev_loop* loop;
    enum evsrv_manager_state state;

    evsrv_manager_on_started_cb on_started;
    evsrv_manager_on_graceful_stop_cb on_graceful_stop;

    evsrv** srvs;
    size_t srvs_len;
    size_t stopped_srvs;
    int active_srvs;
};

void evsrv_manager_init(struct ev_loop* loop, evsrv_manager* self, evsrv_info* servers, size_t servers_count);
void evsrv_manager_destroy(evsrv_manager* self);
void evsrv_manager_bind(evsrv_manager* self);
void evsrv_manager_listen(evsrv_manager* self);
void evsrv_manager_accept(evsrv_manager* self);
void evsrv_manager_stop(evsrv_manager* self);
void evsrv_manager_graceful_stop(evsrv_manager* self, evsrv_manager_on_graceful_stop_cb cb);


#define evsrv_manager_set_on_started(srv, on_started_cb) do { \
    (srv)->on_started = (evsrv_manager_on_started_cb) (on_started_cb); \
} while (0)

EV_CPP(})

#endif //LIBEVSERVER_EVSERVER_H
