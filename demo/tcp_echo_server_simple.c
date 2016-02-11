#include <stdio.h>
#include <stdlib.h>

#include "evsrv.h"

void on_started(evsrv* srv);
void on_read(evsrv_conn* conn, ssize_t nread);
void sigint_cb(struct ev_loop* loop, ev_signal* w, int revents);


int main() {
    ev_signal sig;
    ev_signal_init(&sig, sigint_cb, SIGINT);
    ev_signal_start(EV_DEFAULT, &sig);

    evsrv srv;
    evsrv_init(EV_DEFAULT, &srv, "127.0.0.1", "9090");

    evsrv_set_on_started(&srv, on_started);                                    // will be called on server start
    evsrv_set_on_read(&srv, on_read);                                          // setting on_read callback for every connection

    if (evsrv_bind(&srv) == -1) {                                              // binds to host:port
        return EXIT_FAILURE;
    }
    if (evsrv_listen(&srv) == -1) {                                            // starts listening on host:port
        return EXIT_FAILURE;
    }

    evsrv_accept(&srv);                                                        // beginning to accept connections
    ev_run(srv.loop, 0);

    evsrv_destroy(&srv);                                                         // cleaning evsrv
    ev_loop_destroy(srv.loop);
}

void on_started(evsrv* srv) {
    printf("Started echo demo server at %s:%s\n", srv->host, srv->port);
}

void on_read(evsrv_conn* conn, ssize_t nread) {
    if (nread > 0) {
        evsrv_conn_write(conn, conn->rbuf, (size_t) nread);                    // just replying with what we got
        conn->ruse = 0;                                                        // setting ruse to 0, in order to not exceed read buffer size in future
    }
}

void sigint_cb(struct ev_loop* loop, ev_signal* w, int revents) {
    ev_signal_stop(loop, w);
    ev_break(loop, EVBREAK_ALL);
}
