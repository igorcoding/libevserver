#include "evsrv.h"

#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "util.h"

static void _evsrv_accept_cb(struct ev_loop* loop, ev_io* w, int revents);

/*************************** evsrv ***************************/

#define evsrv_is_tcp(self) self->proto == EVSRV_PROTO_TCP
#define evsrv_is_udp(self) self->proto == EVSRV_PROTO_UDP

void evsrv_init(evsrv* self, enum evsrv_proto proto, const char* host, const char* port) {
    self->loop = EV_DEFAULT;
    self->manager = NULL;
    self->id = 0;
    self->proto = proto;
    self->host = host;
    self->port = port;
    self->state = EVSRV_IDLE;
    self->read_timeout = 0;
    self->write_timeout = 1.0;
    self->backlog = SOMAXCONN;
    self->sock = -1;
    self->active_connections = 0;

    self->on_started = NULL;
    self->on_conn_create = NULL;
    self->on_conn_ready = NULL;
    self->on_conn_close = NULL;
    self->on_read = NULL;
    self->on_graceful_stop = NULL;

    self->connections_len = (size_t) sysconf(_SC_OPEN_MAX);
    self->connections = (evsrv_conn**) calloc(self->connections_len, sizeof(evsrv_conn*));
    memset(self->connections, 0, self->connections_len * sizeof(evsrv_conn*));

    self->data = NULL;
}


void evsrv_clean(evsrv* self) {
    if (self->state != EVSRV_STOPPED) {
        evsrv_stop(self);
    }
    free(self->connections);
    self->manager = NULL;
}

int evsrv_listen(evsrv* self) {

    if (strncasecmp(self->host, "unix/", 5) == 0) { // unix domain socket
        size_t path_len = strlen(self->port);
        if (path_len > 107) {
            cwarn("Too long unix socket path. Max is 107 chars.");
            return -1;
        }
        unlink(self->port);

        struct sockaddr_un* serv_addr = (struct sockaddr_un*) &self->sockaddr.ss;

        memset(serv_addr, 0, sizeof(*serv_addr));
        serv_addr->sun_family = AF_UNIX;
        strncpy(serv_addr->sun_path, self->port, path_len);
        serv_addr->sun_path[path_len] = '\0';

        self->sockaddr.slen = sizeof(struct sockaddr_un);

    } else { // ipv4 socket
        struct sockaddr_in* serv_addr = (struct sockaddr_in*) &self->sockaddr.ss;

        memset(serv_addr, 0, sizeof(*serv_addr));
        serv_addr->sin_family = AF_INET;
        serv_addr->sin_addr.s_addr = inet_addr(self->host);
        serv_addr->sin_port = htons((uint16_t) atoi(self->port));

        self->sockaddr.slen = sizeof(struct sockaddr_in);
    }

    self->sock = socket(self->sockaddr.ss.ss_family, SOCK_STREAM, IPPROTO_TCP);
    ev_io_init(&self->accept_rw, _evsrv_accept_cb, self->sock, EV_READ);

    if (self->sock < 0) {
        cerror("Error creating socket");
        return -1;
    }
    if (evsrv_socket_set_nonblock(self->sock) < 0) {
        cerror("Error setting socket %d to nonblock", self->sock);
    }

    int one = 1;
    if (setsockopt(self->sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        cerror("Error setting socket options: SO_REUSEADDR");
    }

    if (evsrv_is_tcp(self)) {
#if EVSRV_USE_TCP_NO_DELAY != 0
        if (self->sockaddr.ss.ss_family == AF_INET || self->sockaddr.ss.ss_family == AF_INET6) {
            if (setsockopt(self->sock, SOL_TCP, TCP_NODELAY, &one, sizeof(one)) < 0) {
                cerror("Error setting socket options: TCP_NODELAY");
            }
        }
#endif
        if (self->sockaddr.ss.ss_family == AF_INET || self->sockaddr.ss.ss_family == AF_INET6) {
            if (setsockopt(self->sock, SOL_TCP, TCP_DEFER_ACCEPT, &one, sizeof(one)) < 0) {
                cerror("Error setting socket options: TCP_DEFER_ACCEPT");
            }
        }
    }

    struct linger linger = { 1, 0 };
    if (setsockopt(self->sock, SOL_SOCKET, SO_LINGER, &linger, (socklen_t) sizeof(linger)) < 0) {
        cerror("Error setting socket options: SO_LINGER");
    }

    if (bind(self->sock, (struct sockaddr*) &self->sockaddr.ss, self->sockaddr.slen) < 0) {
        cerror("Bind error");
        return -1;
    }

    if (evsrv_is_tcp(self)) {
        if (listen(self->sock, self->backlog) < 0) {
            cerror("Listen error");
            return -1;
        }
    }

    self->state = EVSRV_LISTENING;
    return 0;
}

int evsrv_accept(evsrv* self) {
    ev_io_start(self->loop, &self->accept_rw);
    self->state = EVSRV_ACCEPTING;
    if (self->on_started) {
        self->on_started(self);
    }
    return 0;
}

void _evsrv_accept_cb(struct ev_loop* loop, ev_io* w, int revents) {
    evsrv* self = SELFby(w, evsrv, accept_rw);
    if (EV_ERROR & revents) {
        cerror("error occured on accept");
        return;
    }

    while (1) {
        struct evsrv_sockaddr conn_addr;
        conn_addr.slen = sizeof(conn_addr.ss);
        int conn_sock;

        again:
        conn_sock = accept(w->fd, (struct sockaddr*) &conn_addr.ss, &conn_addr.slen);

        if (conn_sock < 0) {
            switch (errno) {
                case EAGAIN:
                    return;
                case EINTR:
                    goto again;
                default:
                    cerror("accept error");
                    break;
            }
            return;
        }

        struct evsrv_conn_info* conn_info = (struct evsrv_conn_info*) malloc(sizeof(struct evsrv_conn_info));
        conn_info->sock = conn_sock;
        conn_info->addr = conn_addr;
        ++self->active_connections;

        evsrv_conn* conn = NULL;
        if (self->on_conn_create) {
            conn = self->on_conn_create(self, conn_info);
        } else {
            conn = (evsrv_conn*) malloc(sizeof(evsrv_conn));
            evsrv_conn_init(conn, self, conn_info);
            conn->on_read = self->on_read;
            conn->rbuf = (int8_t*) malloc(EVSRV_DEFAULT_BUF_LEN * sizeof(int8_t));
            conn->rlen = EVSRV_DEFAULT_BUF_LEN;
        }

        if (unlikely(self->connections[conn_info->sock] != NULL)) {
            evsrv_conn_close(self->connections[conn_info->sock], 0);
        }
        self->connections[conn_info->sock] = conn;

        evsrv_conn_start(conn);

        if (self->on_conn_ready) {
            self->on_conn_ready(conn);
        }
    }
}

void evsrv_stop(evsrv* self) {
    evsrv_stop_io(self->loop, &self->accept_rw);

    if (self->sock > 0) {
        close(self->sock);
        self->sock = -1;
    }

    for (size_t i = 0; i < self->connections_len; ++i) {
        evsrv_conn* conn = self->connections[i];
        if (conn != NULL) {
            evsrv_conn_close(conn, 0);
        }
    }
    self->state = EVSRV_STOPPED;
}

void evsrv_graceful_stop(evsrv* self, c_cb_evsrv_graceful_stop_t cb) {
    cdebug("graceful stop started");
    evsrv_stop_io(self->loop, &self->accept_rw);

    if (self->sock > 0) {
        close(self->sock);
        self->sock = -1;
    }

    self->state = EVSRV_GRACEFULLY_STOPPING;
    if (self->active_connections == 0) {
        self->state = EVSRV_STOPPED;
        cb(self);
    } else {
        self->on_graceful_stop = cb;
        for (size_t i = 0; i < self->connections_len; ++i) {
            evsrv_conn* conn = self->connections[i];
            if (conn != NULL) {
                int sock = conn->info->sock;
                cdebug("[%d] <on_graceful_stop>", sock);

                bool closed = true;
                if (conn->on_graceful_close) {
                    closed = conn->on_graceful_close(conn);
                    if (!closed) {
                        conn->state = EVSRV_CONN_PENDING_CLOSE;
                    }
                } else {
                    evsrv_conn_close(conn, 0);
                }

                cdebug("[%d] </on_graceful_stop>", sock);

                if (closed && self->active_connections == 0) {
                    self->state = EVSRV_STOPPED;
                    self->on_graceful_stop(self);
                    break;
                }
            }
        }
    }
}
