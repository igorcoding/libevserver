#include <stdio.h>
#include <stdlib.h>

#include "evsrv.h"


// We define a custom connection with some extra data.
// Just for an example there will be a message, which will be send to client.
typedef struct {
    evsrv_conn conn;
    char* message;
} tcpserver_conn;

void on_started(evsrv* srv);
evsrv_conn* on_conn_create(evsrv* srv, struct evsrv_conn_info* info);
void on_conn_close(evsrv_conn* conn, int err);
void on_read(evsrv_conn* conn, ssize_t nread);
void sigint_cb(struct ev_loop* loop, ev_signal* w, int revents);


int main() {
    struct ev_loop* loop = EV_DEFAULT;

    evsrv srv;
    evsrv_init(&srv, EVSRV_PROTO_TCP, "127.0.0.1", "9090");
    srv.loop = loop;

    ev_signal sig;
    ev_signal_init(&sig, sigint_cb, SIGINT);

    srv.on_started = (c_cb_started_t) on_started;                          // will be called on server start

    // to enable custom connections creation we need to define on_conn_create and on_conn_close functions
    srv.on_conn_create = (c_cb_conn_create_t) on_conn_create;
    srv.on_conn_close = (c_cb_conn_close_t) on_conn_close;

    if (evsrv_listen(&srv) != -1) {                                        // binds and starts listening on host:port
        ev_signal_start(srv.loop, &sig);
        evsrv_accept(&srv);                                                // beginning to accept connections
        ev_run(loop, 0);
    }
    evsrv_clean(&srv);                                                     // cleaning evsrv
    ev_loop_destroy(srv.loop);
}

void on_started(evsrv* srv) {
    printf("Started demo server at %s:%s\n", srv->host, srv->port);
}

evsrv_conn* on_conn_create(evsrv* srv, struct evsrv_conn_info* info) {
    tcpserver_conn* c = (tcpserver_conn*) malloc(sizeof(tcpserver_conn));  // allocating memory for our connection
    evsrv_conn_init(&c->conn, srv, info);                                  // initializing evsrv_conn

    c->conn.rbuf = (char*) malloc(1024);                                   // allocating buffer with size 1024
    c->conn.rlen = 1024;                                                   // make sure that you save the buffer size in rlen
    c->conn.on_read = (c_cb_read_t) on_read;                               // setting on_read callback for this connection

    c->message = "Hello from TCP server!";                                 // here goes our custom message

    return (evsrv_conn*) c;                                                // returning newly created connection
}

void on_conn_close(evsrv_conn* conn, int err) {
    tcpserver_conn* c = (tcpserver_conn*) conn;                            // casting evsrv_conn* to our connection type
    evsrv_conn_clean(&c->conn);                                            // cleaning evsrv_conn

    free(c->conn.rbuf);                                                    // cleaning previously allocated buffer
    c->conn.rbuf = NULL;
    free(c);                                                               // cleaning an entire structure
}

void on_read(evsrv_conn* conn, ssize_t nread) {
    tcpserver_conn* c = (tcpserver_conn*) conn;

    evsrv_conn_write(conn, c->message, 0);                                 // just replying with out custom message
    conn->ruse = 0;                                                        // setting ruse to 0, in order to not exceed read buffer size in future
}

void sigint_cb(struct ev_loop* loop, ev_signal* w, int revents) {
    ev_signal_stop(loop, w);
    ev_break(loop, EVBREAK_ALL);
}
