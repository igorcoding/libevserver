#include <stdio.h>

#include <evserver.h>
#include <stdlib.h>

typedef struct {
    evsrv_conn conn;
} my_conn;

static void on_read(evsrv_conn* conn, ssize_t nread);

static evsrv_conn* on_conn_create(evsrv* srv, evsrv_conn_info* info) {
    my_conn* c = (my_conn*) malloc(sizeof(my_conn));
    evsrv_conn_init(&c->conn, srv, info);

    c->conn.rbuf = (char*) malloc(EVSRV_DEFAULT_BUF_LEN);
    c->conn.rlen = EVSRV_DEFAULT_BUF_LEN;
    c->conn.on_read = (c_cb_read_t) on_read;
    return (evsrv_conn*) c;
}

static void on_conn_close(evsrv_conn* conn, int err) {
    my_conn* c = (my_conn*) conn;
    evsrv_conn_clean(&c->conn);
    free(c);
}

void on_read(evsrv_conn* conn, ssize_t nread) {
    printf("%.*s", (int) nread, conn->rbuf + (conn->ruse - (int) nread));
    evsrv_write(conn, "hello, there!\n", 0);
}

int main() {
    evsrv srv;
    evsrv_init(&srv);

    srv.host = "127.0.0.1";
    srv.port = 9090;
    srv.on_conn_create = (c_cb_conn_create_t) on_conn_create;
    srv.on_conn_close = (c_cb_conn_close_t) on_conn_close;
    srv.on_read = (c_cb_read_t) on_read;

    if (evsrv_listen(&srv) != -1) {
        printf("Started demo server at %s:%u\n", srv.host, srv.port);
        evsrv_accept(&srv);
        evsrv_run(&srv);
    }

}