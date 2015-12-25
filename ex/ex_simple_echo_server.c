#include <stdio.h>

#include "evserver.h"

void on_read(evsrv_conn* conn, ssize_t nread) {
    char* rbuf = conn->rbuf;

    evsrv_write(conn, rbuf, conn->ruse);
    conn->ruse = 0;
}

void on_started(evsrv* srv) {
    printf("Started echo demo server at %s:%s\n", srv->host, srv->port);
}

static void signal_cb(struct ev_loop* loop, ev_signal* w, int revents) {
    ev_signal_stop(loop, w);
    evsrv* srv = (evsrv*) w->data;
    evsrv_stop(srv);
    evsrv_clean(srv);
    ev_loop_destroy(loop);
}

int main() {
    evsrv srv;
    evsrv_init(&srv, 1, "127.0.0.1", "9090");

    ev_signal sig;
    ev_signal_init(&sig, signal_cb, SIGINT);
    sig.data = (void*) &srv;

    srv.on_started = (c_cb_started_t) on_started;
    srv.on_read = (c_cb_read_t) on_read;

    if (evsrv_listen(&srv) != -1) {
        ev_signal_start(srv.loop, &sig);
        evsrv_accept(&srv);
        evsrv_run(&srv);
    }
}
