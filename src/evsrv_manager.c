#include <sys/socket.h>
#include <stdlib.h>
#include <sys/un.h>
#include <assert.h>

#include "evsrv_manager.h"
#include "util.h"

static void _evsrv_manager_graceful_stop_cb(evsrv* stopped_srv);


/*************************** evsrv_manager ***************************/

void evsrv_manager_init(struct ev_loop* loop, evsrv_manager* self, evsrv_info* servers, size_t servers_count) {
    self->loop = loop;
    self->srvs_len = servers_count;
    self->srvs = (evsrv**) calloc(self->srvs_len, sizeof(evsrv*));
    for (size_t i = 0; i < self->srvs_len; ++i) {
        size_t id = i + 1;
        self->srvs[i] = servers[i].on_create(self, id, &servers[i]);
        self->srvs[i]->id = id;
        self->srvs[i]->manager = self;
        self->srvs[i]->on_destroy = servers[i].on_destroy;
    }
    self->active_srvs = 0;
    self->stopped_srvs = 0;
    self->on_started = NULL;
    self->on_graceful_stop = NULL;
    self->state = EVSRV_MANAGER_IDLE;
}

void evsrv_manager_destroy(evsrv_manager* self) {
    for (size_t i = 0; i < self->srvs_len; ++i) {
        self->srvs[i]->on_destroy(self->srvs[i]); // freeing
        self->srvs[i] = NULL;
    }
    free(self->srvs);
    self->srvs = NULL;
    self->srvs_len = 0;
    self->active_srvs = 0;
}

void evsrv_manager_bind(evsrv_manager* self) {
    for (size_t i = 0; i < self->srvs_len; ++i) {
        evsrv* srv = self->srvs[i];
        if (evsrv_bind(srv) == -1) {
            cerror("Bind of server [#%lu] %s:%s failed", srv->id, srv->host, srv->port);
        }
    }
    self->state = EVSRV_MANAGER_BOUND;
}

void evsrv_manager_listen(evsrv_manager* self) {
    for (size_t i = 0; i < self->srvs_len; ++i) {
        evsrv* srv = self->srvs[i];
        if (srv->state == EVSRV_BOUND) {
            if (evsrv_listen(srv) == -1) {
                cerror("Listen of server [#%lu] %s:%s failed", srv->id, srv->host, srv->port);
            }
        }
    }
    self->state = EVSRV_MANAGER_LISTENING;
}

void evsrv_manager_accept(evsrv_manager* self) {
    for (size_t i = 0; i < self->srvs_len; ++i) {
        if (self->srvs[i]->state == EVSRV_LISTENING) {
            evsrv_accept(self->srvs[i]);
            ++self->active_srvs;
        }
    }
    if (self->active_srvs > 0) {
        self->state = EVSRV_MANAGER_ACCEPTING;
        if (self->on_started) {
            self->on_started(self);
        }
    }
}

void evsrv_manager_stop(evsrv_manager* self) {
    for (size_t i = 0; i < self->srvs_len; ++i) {
        evsrv* srv = self->srvs[i];
        evsrv_stop(srv);
        if (srv->state == EVSRV_ACCEPTING) {
            --self->active_srvs;
        }
        ++self->stopped_srvs;
    }
}

void evsrv_manager_graceful_stop(evsrv_manager* self, evsrv_manager_on_graceful_stop_cb cb) {
    cdebug("evserver graceful stop started");
    self->state = EVSRV_MANAGER_GRACEFULLY_STOPPING;
    self->on_graceful_stop = cb;
    for (size_t i = 0; i < self->srvs_len; ++i) {
        evsrv* srv = self->srvs[i];
        if (srv->state == EVSRV_ACCEPTING) {
            evsrv_graceful_stop(srv, _evsrv_manager_graceful_stop_cb);
        } else {
            evsrv_stop(srv);
            ++self->stopped_srvs;
        }
    }
    if (self->state != EVSRV_MANAGER_STOPPED && self->stopped_srvs == self->srvs_len) {
        self->on_graceful_stop(self);
    }
}

void _evsrv_manager_graceful_stop_cb(evsrv* stopped_srv) {
    evsrv_manager* server = stopped_srv->manager;
    assert("server instance should not be NULL" && server != NULL);
    --server->active_srvs;
    ++server->stopped_srvs;
    if (server->stopped_srvs == server->srvs_len) {
        server->state = EVSRV_MANAGER_STOPPED;
        server->on_graceful_stop(server);
    }
}
