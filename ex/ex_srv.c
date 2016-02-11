#include <stdio.h>

#include "util.h"
#include "evsrv_manager.h"
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

static evsrv_conn* on_conn_create(evsrv* srv, struct evsrv_conn_info* info) {
    my1_conn* c = (my1_conn*) malloc(sizeof(my1_conn));
    evsrv_conn_init(&c->conn, srv, info);

    evsrv_conn_set_rbuf(&c->conn, (char*) malloc(EVSRV_DEFAULT_BUF_LEN), EVSRV_DEFAULT_BUF_LEN);
    evsrv_conn_set_on_read(&c->conn, on_read);
    evsrv_conn_set_on_graceful_close(&c->conn, on_graceful_conn_close);

    c->tw.data = (void*) c;
    ev_timer_init(&c->tw, on_graceful_conn_timeout, 1, 0);

    return (evsrv_conn*) c;
}

static void on_conn_destroy(evsrv_conn* conn, int err) {
    my1_conn* c = (my1_conn*) conn;
    cwarn("[%d] user: on_conn_destroy", c->conn.info->sock);
    evsrv_conn_destroy(&c->conn);
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

        evsrv_conn_write(conn, rbuf, size);
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
    ev_break(srv->loop, EVUNLOOP_ALL);
}

static void signal_cb(struct ev_loop* loop, ev_signal* w, int revents) {
    ev_signal_stop(loop, w);
    evsrv* srv = (evsrv*) w->data;
    evsrv_graceful_stop(srv, on_gracefully_stopped);
}

int main() {
    struct ev_loop* loop = EV_DEFAULT;
    evsrv srv;
    evsrv_init(loop, &srv, "127.0.0.1", "9090");
    // evsrv_init(loop, &srv, "unix/", "/var/tmp/ev_srv.sock");

    srv.write_timeout = 0.0;

    evsrv_set_on_started(&srv, on_started);
    evsrv_set_on_conn(&srv, on_conn_create, on_conn_destroy);
    evsrv_set_on_read(&srv, on_read);

    if (evsrv_bind(&srv) == -1) {
        return EXIT_FAILURE;
    }
    if (evsrv_listen(&srv) == -1) {
        return EXIT_FAILURE;
    }

    ev_signal sig;
    ev_signal_init(&sig, signal_cb, SIGINT);
    sig.data = (void*) &srv;
    ev_signal_start(srv.loop, &sig);

    evsrv_accept(&srv);
    ev_run(loop, 0);
//    int max_childs = 4;
//    for (int i = 0; i < max_childs; ++i) {
//        pid_t pid = fork();
//        if (pid > 0) {
//            // master
//
//        } else if (pid == 0) {
//            // child
//            ev_loop_fork(srv.loop);
//            evsrv_accept(&srv);
//            ev_run(loop, 0);
//            break;
//        } else {
//            // error
//            perror("fork failed");
//            break;
//        }
//    }

    evsrv_destroy(&srv);
    ev_loop_destroy(srv.loop);

}
