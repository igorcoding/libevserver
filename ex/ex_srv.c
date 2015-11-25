#include <stdio.h>

#include <evserver.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

typedef struct {
    evsrv_conn conn;
} my1_conn;

static void on_read(evsrv_conn* conn, ssize_t nread);

static evsrv_conn* on_conn_create(evsrv* srv, evsrv_conn_info* info) {
    my1_conn* c = (my1_conn*) malloc(sizeof(my1_conn));
    evsrv_conn_init(&c->conn, srv, info);

    c->conn.rbuf = (char*) malloc(EVSRV_DEFAULT_BUF_LEN);
    c->conn.rlen = EVSRV_DEFAULT_BUF_LEN;
    c->conn.on_read = (c_cb_read_t) on_read;
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

void on_started(evsrv* srv) {
    printf("Started demo server at %s:%s\n", srv->host, srv->port);
}

int main() {
    evsrv srv;
    evsrv_init(&srv, 1, "127.0.0.1", "9090");

    srv.backlog = 500;
    srv.write_timeout = 0.0;
    srv.on_started = (c_cb_started_t) on_started;
    srv.on_conn_create = (c_cb_conn_create_t) on_conn_create;
    srv.on_conn_close = (c_cb_conn_close_t) on_conn_close;
    srv.on_read = (c_cb_read_t) on_read;

    if (evsrv_listen(&srv) != -1) {
        evsrv_accept(&srv);
        evsrv_run(&srv);
//        int max_childs = 4;
//        for (int i = 0; i < max_childs; ++i) {
//            pid_t pid = fork();
//            if (pid > 0) {
//                // master
//
//            } else if (pid == 0) {
//                // child
//                evsrv_notify_fork_child(&srv);
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
