#include <iostream>
#include <cstdlib>
#include <cstdio>

#include <ev++.h>
#include <evsrv++.h>
#include <evsrv_conn++.h>

class my_conn : public ev::srv_conn {
public:
    my_conn(ev::srv& s, evsrv_conn_info* info)
            : srv_conn(s, info)
    { }
};

void on_started(ev::srv& srv);
ev::srv_conn* on_conn_create(ev::srv& s, evsrv_conn_info* info);
void on_conn_destroy(ev::srv_conn& conn, int err);
void on_read(ev::srv_conn& conn, ssize_t nread);
void sigint_cb(ev::sig& w, int revents);


int main() {
    ev::loop_ref loop = ev::get_default_loop();

    ev::sig sig(loop);
    sig.set<sigint_cb>();
    sig.set(SIGINT);
    sig.start();

    {
        ev::srv s(loop, "127.0.0.1", "9090");
        s.set_on_started<on_started>();
        s.set_on_read<on_read>();
        s.set_on_conn<on_conn_create, on_conn_destroy>();

        if (s.bind() == -1) {
            return EXIT_FAILURE;
        }
        if (s.listen() == -1) {
            return EXIT_FAILURE;
        }

        s.accept();
        loop.run();
    }

    ev_loop_destroy(loop.raw_loop);

}

void on_started(ev::srv& srv) {
    printf("Started echo demo server at %s:%s\n", srv.host(), srv.port());
}

ev::srv_conn* on_conn_create(ev::srv& srv, evsrv_conn_info* info) {
    my_conn* c = new my_conn(srv, info);

    char* buf = new char[1024];
    c->set_rbuf(buf, 1024);
    c->set_on_read<on_read>();
    c->set_write_now(false);
    return c;
}

void on_conn_destroy(ev::srv_conn& conn, int err) {
    my_conn& c = (my_conn&) conn;
    delete[] conn.rbuf();
    conn.set_rbuf(NULL, 0);

    delete &conn;
}

void on_read(ev::srv_conn& conn, ssize_t nread) {
    if (nread <= 0) {
        return;
    }
    my_conn& c = (my_conn&) conn;

    char* rbuf = c.rbuf();
    c.write(rbuf, (size_t) nread);
    c.ruse() = 0;
}

void sigint_cb(ev::sig& w, int revents) {
    w.stop();
    w.loop.break_loop(ev::ALL);
}
