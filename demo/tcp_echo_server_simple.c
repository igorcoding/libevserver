#include <stdio.h>
#include <stdlib.h>

#include "evserver.h"

void on_started(evsrv* srv);
void on_read(evsrv_conn* conn, ssize_t nread);
void sigint_cb(struct ev_loop* loop, ev_signal* w, int revents);


int main() {
    evsrv srv;
    evsrv_init(&srv, 1, "127.0.0.1", "9090");

    ev_signal sig;
    ev_signal_init(&sig, sigint_cb, SIGINT);
    sig.data = (void*) &srv;

    srv.on_started = (c_cb_started_t) on_started;                          // will be called on server start
    srv.on_read = (c_cb_read_t) on_read;                                   // setting on_read callback for every connection

    if (evsrv_listen(&srv) != -1) {                                        // binds and starts listening on host:port
        ev_signal_start(srv.loop, &sig);
        evsrv_accept(&srv);                                                // beginning to accept connections
        ev_run(srv.loop, 0);
    }
}

void on_started(evsrv* srv) {
    printf("Started echo demo server at %s:%s\n", srv->host, srv->port);
}

void on_read(evsrv_conn* conn, ssize_t nread) {
    evsrv_conn_write(conn, conn->rbuf, (size_t) nread);                         // just replying with what we got
    conn->ruse = 0;                                                        // setting ruse to 0, in order to not exceed read buffer size in future
}

void sigint_cb(struct ev_loop* loop, ev_signal* w, int revents) {
    ev_signal_stop(loop, w);
    evsrv* srv = (evsrv*) w->data;
    evsrv_clean(srv);                                                      // cleaning evsrv
    ev_break(loop, EVUNLOOP_ALL);
    ev_loop_destroy(srv->loop);
}
