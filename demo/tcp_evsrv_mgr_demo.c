#include <stdio.h>
#include <stdlib.h>

#include "evsrv_manager.h"

typedef struct {
    evsrv srv;
    char* message;
} server1;

typedef struct {
    evsrv srv;
    char* message;
} server2;

static evsrv* on_server1_create(evsrv_manager* self, size_t id, evsrv_info* info);
static void on_server1_destroy(evsrv* self);
void on_server1_read(evsrv_conn* conn, ssize_t nread);

static evsrv* on_server2_create(evsrv_manager* self, size_t id, evsrv_info* info);
static void on_server2_destroy(evsrv* self);
void on_server2_read(evsrv_conn* conn, ssize_t nread);

void on_started(evsrv_manager* srv);
void sigint_cb(struct ev_loop* loop, ev_signal* w, int revents);


int main() {

    evsrv_info hosts[] = {                                                    // specify all the servers
            { "127.0.0.1", "9090", on_server1_create, on_server1_destroy },
            { "127.0.0.1", "7070", on_server2_create, on_server2_destroy },
    };
    size_t hosts_len = sizeof(hosts) / sizeof(hosts[0]);

    evsrv_manager mgr;
    evsrv_manager_init(&mgr, hosts, hosts_len);
    mgr.on_started = (c_cb_started_t) on_started;                             // will be called when all servers are started

    ev_signal sig;
    ev_signal_init(&sig, sigint_cb, SIGINT);

    evsrv_manager_listen(&mgr);                                               // binds and starts listening every server
    ev_signal_start(mgr.loop, &sig);
    evsrv_manager_accept(&mgr);                                               // beginning to accept connections in every server
    ev_run(mgr.loop, 0);

    evsrv_manager_clean(&mgr);                                                // cleaning evsrv_manager
    ev_loop_destroy(mgr.loop);
}

void on_started(evsrv_manager* srv) {
    printf("Started evsrv_manager\n");
}


// -- Server 1 -- //

evsrv* on_server1_create(evsrv_manager* self, size_t id, evsrv_info* info) {
    server1* s = (server1*) malloc(sizeof(server1));                          // allocating memory for the server
    evsrv_init(&s->srv, info->host, info->port);                              // initializing it with desired options
    s->srv.backlog = SOMAXCONN;
    s->srv.write_timeout = 0.0;
    s->srv.on_read = (c_cb_read_t) on_server1_read;

    s->message = "Hello from server1!";                                       // some custom data
    return (evsrv*) s;
}

void on_server1_destroy(evsrv* self) {
    server1* s = (server1*) self;
    evsrv_clean(&s->srv);                                                     // cleaning srv
    free(s);                                                                  // freeing allocated memory
}

void on_server1_read(evsrv_conn* conn, ssize_t nread) {
    server1* srv = (server1*) conn->srv;
    evsrv_conn_write(conn, srv->message, 0);                                 // replying on each read by our custom message
    conn->ruse = 0;                                                          // setting ruse to 0, in order to not exceed read buffer size in future
}

// -- Server 2 -- //

evsrv* on_server2_create(evsrv_manager* self, size_t id, evsrv_info* info) {
    server2* s = (server2*) malloc(sizeof(server2));                          // allocating memory for the server
    evsrv_init(&s->srv, info->host, info->port);                              // initializing it with desired options
    s->srv.backlog = SOMAXCONN;
    s->srv.write_timeout = 0.0;
    s->srv.on_read = (c_cb_read_t) on_server2_read;

    s->message = "Hello from server2!";                                       // some custom data
    return (evsrv*) s;
}

void on_server2_destroy(evsrv* self) {
    server2* s = (server2*) self;
    evsrv_clean(&s->srv);                                                     // cleaning srv
    free(s);                                                                  // freeing allocated memory
}

void on_server2_read(evsrv_conn* conn, ssize_t nread) {
    server2* srv = (server2*) conn->srv;
    evsrv_conn_write(conn, srv->message, 0);                                 // replying on each read by our custom message
    conn->ruse = 0;                                                          // setting ruse to 0, in order to not exceed read buffer size in future
}

// -- Util -- //

void sigint_cb(struct ev_loop* loop, ev_signal* w, int revents) {
    ev_signal_stop(loop, w);
    ev_break(loop, EVBREAK_ALL);
}
