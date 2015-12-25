#include <stdio.h>

#include "../include/evserver.h"
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

typedef struct {
    evsrv srv;
} my1_srv;

typedef struct {
    evsrv srv;
} my2_srv;

typedef struct {
    evsrv_conn conn;
} my1_conn;




static void on_read(evsrv_conn* conn, ssize_t nread);
static void on_graceful_conn_close(evsrv_conn* conn);

static evsrv_conn* on_conn_create(evsrv* srv, evsrv_conn_info* info) {
    my1_conn* c = (my1_conn*) malloc(sizeof(my1_conn));
    evsrv_conn_init(&c->conn, srv, info);

    c->conn.rbuf = (char*) malloc(EVSRV_DEFAULT_BUF_LEN);
    c->conn.rlen = EVSRV_DEFAULT_BUF_LEN;
    c->conn.on_read = (c_cb_read_t) on_read;
    c->conn.on_graceful_close = (c_cb_graceful_close_t) on_graceful_conn_close;
    return (evsrv_conn*) c;
}

static void on_conn_close(evsrv_conn* conn, int err) {
    my1_conn* c = (my1_conn*) conn;
    evsrv_conn_clean(&c->conn);
    free(c->conn.rbuf);
    c->conn.rbuf = NULL;
    free(c);
}

void on_read(evsrv_conn* conn, ssize_t nread) {
    char* rbuf = conn->rbuf;
    char* end = rbuf + conn->ruse;

    size_t size = 10;

    while (rbuf < end) {
        ptrdiff_t buf_len = end - rbuf;
        if (buf_len < size) {
            break;
        }

        evsrv_write(conn, rbuf, size);
        rbuf += size;

        if (rbuf == end) {
            conn->ruse = 0;
            break;
        }
    }

    // printf("%.*s", (int) nread, buf);
    conn->ruse = (uint32_t) (end - rbuf);
    if (conn->ruse > 0) {
        memmove(conn->rbuf, rbuf, conn->ruse);
    }
}

static void on_graceful_conn_close(evsrv_conn* conn) {
    int sock = conn->info->sock;
    cwarn("[%d] user: on_graceful_conn_close", sock);
    evsrv_conn_close(conn, 0);
    cwarn("[%d] user: done on_graceful_conn_close", sock);
}

void on_started_my1(evsrv* srv) {
    printf("Started demo my1 server at %s:%s\n", srv->host, srv->port);
}

void on_started_my2(evsrv* srv) {
    printf("Started demo my2 server at %s:%s\n", srv->host, srv->port);
}



static evsrv* on_my1_create(evserver* self, size_t id, evserver_info* info) {
    my1_srv* s = (my1_srv*) malloc(sizeof(my1_srv));
    evsrv_init(&s->srv, id, info->host, info->port);
    s->srv.backlog = 500;
    s->srv.write_timeout = 0.0;
    s->srv.on_started = (c_cb_started_t) on_started_my1;
    s->srv.on_conn_create = (c_cb_conn_create_t) on_conn_create;
    s->srv.on_conn_close = (c_cb_conn_close_t) on_conn_close;
    return (evsrv*) s;
}

static void on_my1_destroy(evsrv* self) {
    my1_srv* s = (my1_srv*) self;
    evsrv_clean(&s->srv);
    free(s);
}


static evsrv* on_my2_create(evserver* self, size_t id, evserver_info* info) {
    my2_srv* s = (my2_srv*) malloc(sizeof(my1_srv));
    evsrv_init(&s->srv, id, info->host, info->port);
    s->srv.backlog = 500;
    s->srv.write_timeout = 0.0;
    s->srv.on_started = (c_cb_started_t) on_started_my2;
    s->srv.on_conn_create = (c_cb_conn_create_t) on_conn_create;
    s->srv.on_conn_close = (c_cb_conn_close_t) on_conn_close;
    return (evsrv*) s;
}

static void on_my2_destroy(evsrv* self) {
    my2_srv* s = (my2_srv*) self;
    evsrv_clean(&s->srv);
    free(s);
}

static void on_gracefully_stopped(evserver* server) {
    cwarn("Gracefully stopped evserver");
    evserver_clean(server);
    ev_loop_destroy(server->loop);
}


static void signal_cb(struct ev_loop* loop, ev_signal* w, int revents) {
    ev_signal_stop(loop, w);
    evserver* server = (evserver*) w->data;
    evserver_graceful_stop(server, on_gracefully_stopped);
}



int main() {

    evserver_info hosts[] = {
            { "127.0.0.1", "9090", on_my1_create, on_my1_destroy },
            { "127.0.0.1", "7070", on_my2_create, on_my2_destroy },
    };
    size_t hosts_len = sizeof(hosts) / sizeof(hosts[0]);

    evserver server;
    evserver_init(&server, hosts, hosts_len);

    ev_signal sig;
    ev_signal_init(&sig, signal_cb, SIGINT);
    sig.data = (void*) &server;


    evserver_listen(&server);
    ev_signal_start(server.loop, &sig);
    evserver_accept(&server);
    ev_run(server.loop, 0);


//    int max_childs = 8;
//    for (int i = 0; i < max_childs; ++i) {
//        pid_t pid = fork();
//        if (pid > 0) {
//            // master
//
//        } else if (pid == 0) {
//            // child
//            evserver_notify_fork_child(&server);
//            evserver_accept(&server);
//            evserver_run(&server);
//            break;
//        } else {
//            // error
//            perror("fork failed");
//            break;
//        }
//    }

}
