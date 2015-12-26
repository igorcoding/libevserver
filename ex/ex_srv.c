#include <stdio.h>

#define DEBUG 1
#include "util.h"
#include "evserver.h"
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

typedef struct {
    evsrv_conn conn;
    ev_timer tw;
} my1_conn;

static void on_read(evsrv_conn* conn, ssize_t nread);
static bool on_graceful_conn_close(evsrv_conn* conn);
static void on_graceful_conn_timeout(struct ev_loop* loop, ev_timer* timer, int revents) {
    ev_timer_stop(loop, timer);

    my1_conn* c = (my1_conn*) timer->data;
    cwarn("timered. closing %d", c->conn.info->sock);
    evsrv_conn_close(&c->conn, 0);
}

static evsrv_conn* on_conn_create(evsrv* srv, evsrv_conn_info* info) {
    my1_conn* c = (my1_conn*) malloc(sizeof(my1_conn));
    evsrv_conn_init(&c->conn, srv, info);

    c->conn.rbuf = (char*) malloc(EVSRV_DEFAULT_BUF_LEN);
    c->conn.rlen = EVSRV_DEFAULT_BUF_LEN;
    c->conn.on_read = (c_cb_read_t) on_read;
    c->conn.on_graceful_close = (c_cb_graceful_close_t) on_graceful_conn_close;

    c->tw.data = (void*) c;
    ev_timer_init(&c->tw, on_graceful_conn_timeout, 1, 0);

    return (evsrv_conn*) c;
}

static void on_conn_close(evsrv_conn* conn, int err) {
    my1_conn* c = (my1_conn*) conn;
    cwarn("[%d] user: on_conn_close", c->conn.info->sock);
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

static bool on_graceful_conn_close(evsrv_conn* conn) {
    my1_conn* c = (my1_conn*) conn;
    int sock = conn->info->sock;
    cwarn("[%d] user: on_graceful_conn_close", sock);
//    evsrv_conn_close(conn, 0);
    ev_timer_start(conn->srv->loop, &c->tw);
    cwarn("[%d] user: done on_graceful_conn_close", sock);
    return false;
}

void on_started(evsrv* srv) {
    printf("Started demo server at %s:%s\n", srv->host, srv->port);
}


static void on_gracefully_stopped(evsrv* srv) {
    cwarn("Gracefully stopped %s:%s", srv->host, srv->port);
    evsrv_clean(srv);
    ev_break(srv->loop, EVUNLOOP_ALL);
    ev_loop_destroy(srv->loop);
}

static void signal_cb(struct ev_loop* loop, ev_signal* w, int revents) {
    ev_signal_stop(loop, w);
    evsrv* srv = (evsrv*) w->data;
//    evsrv_stop(srv);
//    evsrv_clean(srv);
    evsrv_graceful_stop(srv, on_gracefully_stopped);
//    ev_loop_destroy(srv->loop);
}

int main() {
    struct ev_loop* loop = EV_DEFAULT;
    evsrv srv;
    evsrv_init(&srv, 1, "127.0.0.1", "9090");
//    evsrv_init(&srv, 1, "unix/", "/var/tmp/ev_srv.sock");
    srv.loop = loop;

    ev_signal sig;
    ev_signal_init(&sig, signal_cb, SIGINT);
    sig.data = (void*) &srv;

    srv.write_timeout = 0.0;
    srv.on_started = (c_cb_started_t) on_started;
    srv.on_conn_create = (c_cb_conn_create_t) on_conn_create;
    srv.on_conn_close = (c_cb_conn_close_t) on_conn_close;
    srv.on_read = (c_cb_read_t) on_read;

    if (evsrv_listen(&srv) != -1) {
        ev_signal_start(srv.loop, &sig);
        evsrv_accept(&srv);
        ev_run(loop, 0);
//        int max_childs = 4;
//        for (int i = 0; i < max_childs; ++i) {
//            pid_t pid = fork();
//            if (pid > 0) {
//                // master
//
//            } else if (pid == 0) {
//                // child
//                ev_loop_fork(self->loop);
//                evsrv_accept(&srv);
//                evsrv_run(&srv);
//                break;
//            } else {
//                // error
//                perror("fork failed");
//                break;
//            }
//        }
    }

}
