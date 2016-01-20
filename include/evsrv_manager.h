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

typedef evsrv* (* c_cb_srv_create_t)(evsrv_manager*, size_t, evsrv_info*);
typedef void   (* c_cb_evsrv_manager_graceful_stop_t)(evsrv_manager*);

struct _evsrv_info {
    // enum evsrv_proto proto;
    char* host;
    char* port;

    c_cb_srv_create_t on_create;
    c_cb_srv_destroy_t on_destroy;
};

enum evsrv_manager_state {
    EVSRV_MANAGER_IDLE,
    EVSRV_MANAGER_LISTENING,
    EVSRV_MANAGER_ACCEPTING,
    EVSRV_MANAGER_GRACEFULLY_STOPPING,
    EVSRV_MANAGER_STOPPED
};

struct _evsrv_manager {
    struct ev_loop* loop;
    enum evsrv_manager_state state;

    c_cb_started_t on_started;
    c_cb_evsrv_manager_graceful_stop_t on_graceful_stop;

    evsrv** srvs;
    size_t srvs_len;
    size_t stopped_srvs;
    int active_srvs;
};

void evsrv_manager_init(evsrv_manager* self, evsrv_info* servers, size_t servers_count);
void evsrv_manager_clean(evsrv_manager* self);
void evsrv_manager_listen(evsrv_manager* self);
void evsrv_manager_accept(evsrv_manager* self);
void evsrv_manager_stop(evsrv_manager* self);
void evsrv_manager_graceful_stop(evsrv_manager* self, c_cb_evsrv_manager_graceful_stop_t cb);

EV_CPP(})

#endif //LIBEVSERVER_EVSERVER_H
