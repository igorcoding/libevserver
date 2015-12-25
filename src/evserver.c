#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <stddef.h>
#include <strings.h>
#include <assert.h>

#include "evserver.h"
#include "evsrv_sockaddr.h"

#ifndef IOV_MAX
#  ifdef UIO_MAXIOV
#    define IOV_MAX UIO_MAXIOV
#  else
#    define IOV_MAX 1024
#  endif
#endif

static void _evsrv_accept_cb(struct ev_loop* loop, ev_io* w, int revents);

static void _evsrv_conn_read_cb(struct ev_loop* loop, ev_io* w, int revents);
static void _evsrv_conn_read_timeout_cb(struct ev_loop* loop, ev_timer* w, int revents);

static void _evsrv_conn_write_cb(struct ev_loop* loop, ev_io* w, int revents);
static void _evsrv_conn_write_timeout_cb(struct ev_loop* loop, ev_timer* w, int revents);

static void _evserver_graceful_stop_cb(evsrv* stopped_srv);


/*************************** evserver ***************************/

void evserver_init(evserver* self, evserver_info* servers, size_t servers_count) {
    self->loop = EV_DEFAULT;
    self->srvs_len = servers_count;
    self->srvs = (evsrv**) calloc(self->srvs_len, sizeof(evsrv*));
    for (size_t i = 0; i < self->srvs_len; ++i) {
        self->srvs[i] = servers[i].on_create(self, i, &servers[i]);
        self->srvs[i]->loop = self->loop;
        self->srvs[i]->server = self;
        self->srvs[i]->on_destroy = servers[i].on_destroy;
    }
    self->active_srvs = 0;
    self->on_started = NULL;
    self->on_graceful_stop = NULL;
}

void evserver_clean(evserver* self) {
    for (size_t i = 0; i < self->srvs_len; ++i) {
        self->srvs[i]->on_destroy(self->srvs[i]); // freeing
        self->srvs[i] = NULL;
    }
    free(self->srvs);
    self->srvs_len = 0;
}

void evserver_listen(evserver* self) {
    for (size_t i = 0; i < self->srvs_len; ++i) {
        evsrv* srv = self->srvs[i];
        if (evsrv_listen(srv) == -1) {
            cerror("Listen of server [#%lu] %s:%s failed", srv->id, srv->host, srv->port);
        }
    }
}

void evserver_accept(evserver* self) {
    for (size_t i = 0; i < self->srvs_len; ++i) {
        if (self->srvs[i]->state == EVSRV_LISTENING) {
            evsrv_accept(self->srvs[i]);
            ++self->active_srvs;
        }
    }
    if (self->on_started) {
        self->on_started(self);
    }
}

void evserver_notify_fork_child(evserver* self) {
    for (size_t i = 0; i < self->srvs_len; ++i) {
        evsrv* srv = self->srvs[i];
        evsrv_notify_fork_child(srv);
    }
}


void evserver_run(evserver* self) {
    ev_run(self->loop, 0);
}

void evserver_stop(evserver* self) {
    for (size_t i = 0; i < self->srvs_len; ++i) {
        evsrv* srv = self->srvs[i];
        evsrv_stop(srv);
        --self->active_srvs;
    }
}

void evserver_graceful_stop(evserver* self, c_cb_evserver_graceful_stop_t cb) {
    cwarn("evserver graceful stop started");
    self->on_graceful_stop = cb;
    for (size_t i = 0; i < self->srvs_len; ++i) {
        evsrv* srv = self->srvs[i];
        if (srv->state == EVSRV_ACCEPTING) {
            evsrv_graceful_stop(srv, _evserver_graceful_stop_cb);
        } else {
            evsrv_stop(srv);
            --self->active_srvs;
            if (self->active_srvs == 0) {
                self->on_graceful_stop(self);
            }
        }
    }
}

void _evserver_graceful_stop_cb(evsrv* stopped_srv) {
    evserver* server = stopped_srv->server;
    assert(server != NULL);
    --server->active_srvs;
    if (server->active_srvs == 0) {
        server->on_graceful_stop(server);
    }
}

/*************************** evsrv ***************************/

void evsrv_init(evsrv* self, size_t id, const char* host, const char* port) {
    self->loop = EV_DEFAULT;
    self->server = NULL;
    self->id = id;
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
    evsrv_stop_io(self->loop, &self->accept_rw);


    if (self->sock > 0) {
        close(self->sock);
        self->sock = -1;
    }

    for (size_t i = 0; i < self->connections_len; ++i) {
        if (self->connections[i] != NULL) {
            evsrv_conn* conn = self->connections[i];
            evsrv_conn_stop(conn);
            evsrv_conn_clean(conn);
            free(conn);
            self->connections[i] = NULL;
        }
    }
    free(self->connections);
    self->server = NULL;
}

int evsrv_listen(evsrv* self) {

    if (strncasecmp(self->host, "unix/", 5) == 0) { // unix domain socket
        size_t path_len = strlen(self->port);
        if (path_len >= 108) {
            cwarn("Too long unix socket path. Max is 107 chars.");
            return -1;
        }
        unlink(self->port);

        struct sockaddr_un* serv_addr = (struct sockaddr_un*) &self->sockaddr.ss;

        memset((char*) serv_addr, 0, sizeof(*serv_addr));
        serv_addr->sun_family = AF_UNIX;
        strncpy(serv_addr->sun_path, self->port, path_len);
        serv_addr->sun_path[path_len] = '\0';

        self->sockaddr.slen = sizeof(struct sockaddr_un);

    } else { // ipv4 socket
        struct sockaddr_in* serv_addr = (struct sockaddr_in*) &self->sockaddr.ss;

        memset((char*) serv_addr, 0, sizeof(*serv_addr));
        serv_addr->sin_family = AF_INET;
        serv_addr->sin_addr.s_addr = inet_addr(self->host);
        serv_addr->sin_port = htons((uint16_t) atoi(self->port));

        self->sockaddr.slen = sizeof(struct sockaddr_in);
    }

    self->sock = socket(self->sockaddr.ss.ss_family, SOCK_STREAM, IPPROTO_TCP);

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

    struct linger linger = { 1, 0 };
    if (setsockopt(self->sock, SOL_SOCKET, SO_LINGER, &linger, (socklen_t) sizeof(linger)) < 0) {
        cerror("Error setting socket options: SO_LINGER");
    }

    if (bind(self->sock, (struct sockaddr*) &self->sockaddr.ss, self->sockaddr.slen) < 0) {
        cerror("Bind error");
        return -1;
    }

    if (listen(self->sock, self->backlog) < 0) {
        cerror("Listen error");
        return -1;
    }

    self->state = EVSRV_LISTENING;
    return 0;
}

int evsrv_accept(evsrv* self) {
    ev_io_init(&self->accept_rw, _evsrv_accept_cb, self->sock, EV_READ);
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

        evsrv_conn_info* conn_info = (evsrv_conn_info*) malloc(sizeof(evsrv_conn_info));
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
            conn->rbuf = (char*) malloc(EVSRV_DEFAULT_BUF_LEN);
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

void evsrv_notify_fork_child(evsrv* self) {
    ev_loop_fork(self->loop);
}

void evsrv_run(evsrv* self) {
    ev_run(self->loop, 0);
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
}

void evsrv_graceful_stop(evsrv* self, c_cb_evsrv_graceful_stop_t cb) {
    cwarn("graceful stop started");
    evsrv_stop_io(self->loop, &self->accept_rw);

    if (self->sock > 0) {
        close(self->sock);
        self->sock = -1;
    }


    self->state = EVSRV_STOPPING;
    if (self->active_connections == 0) {
        cb(self);
    } else {
        self->on_graceful_stop = cb;
        for (size_t i = 0; i < self->connections_len; ++i) {
            evsrv_conn* conn = self->connections[i];
            if (conn != NULL) {
                int sock = conn->info->sock;
                cwarn("[%d] <on_graceful_stop>", sock);
                conn->on_graceful_close(conn);
                cwarn("[%d] </on_graceful_stop>", sock);
            }

            if (self->active_connections == 0 && self->state == EVSRV_STOPPING) {
                cwarn("after graceful shutdown");
                self->state = EVSRV_STOPPED;
                self->on_graceful_stop(self);
                break;
            }
        }
    }
}




/*************************** evsrv_conn ***************************/

void evsrv_conn_init(evsrv_conn* self, evsrv* srv, evsrv_conn_info* info) {
    self->srv = srv;
    self->info = info;
    self->rbuf = NULL;
    self->ruse = 0;
    self->rlen = 0;
    self->wnow = 1;
    self->state = EVSRV_CONN_CREATED;

    self->wuse = 0;
    self->wlen = 0;
    self->wbuf = NULL;

    self->on_read = NULL;
    self->on_graceful_close = NULL;

    if (evsrv_socket_set_nonblock(self->info->sock) < 0) {
        cerror("Error setting O_NONBLOCK");
    }
    struct linger linger = { 1, 0 };
    if (setsockopt(self->info->sock, SOL_SOCKET, SO_LINGER, &linger, (socklen_t) sizeof(linger)) < 0) {
        cerror("Error setting socket options");
    }
}

void evsrv_conn_start(evsrv_conn* self) {
    ev_io_init(&self->rw, _evsrv_conn_read_cb, self->info->sock, EV_READ);
    ev_io_start(self->srv->loop, &self->rw);

    ev_timer_init(&self->trw, _evsrv_conn_read_timeout_cb, self->srv->read_timeout, 0);
    if (unlikely(self->srv->read_timeout > 0)) {
        ev_timer_start(self->srv->loop, &self->trw);
    }

    ev_io_init(&self->ww, _evsrv_conn_write_cb, self->info->sock, EV_WRITE);
    ev_timer_init(&self->tww, _evsrv_conn_write_timeout_cb, self->srv->write_timeout, 0);
    self->state = EVSRV_CONN_ACTIVE;
}

void evsrv_conn_stop(evsrv_conn* self) {
    evsrv_stop_io(self->srv->loop, &self->rw);
    evsrv_stop_timer(self->srv->loop, &self->trw);
    evsrv_stop_io(self->srv->loop, &self->ww);
    evsrv_stop_timer(self->srv->loop, &self->tww);

    self->state = EVSRV_CONN_STOPPED;

}

void evsrv_conn_clean(evsrv_conn* self) {
    cwarn("[%d] evsrv_conn_clean", self->info->sock);
    if (self->info->sock > -1) {
        close(self->info->sock);
        self->info->sock = -1;
    }

    free(self->info);
    self->info = NULL;

    // cleanup of rbuf should be performed by the allocator (who allocated)
    self->ruse = 0;
    self->rlen = 0;

    for (size_t i = 0; i < self->wuse; ++i) {
        free(self->wbuf[i].iov_base);
    }
    free(self->wbuf);
    self->wbuf = NULL;
    self->wuse = 0;
    self->wlen = 0;

    self->srv = NULL;
}

void evsrv_conn_shutdown(evsrv_conn* self, int how) {
    if (self->info->sock > -1) {
        self->state = EVSRV_CONN_SHUTDOWN;
        shutdown(self->info->sock, how);
    }
}

void evsrv_conn_close(evsrv_conn* self, int err) {
    int sock = self->info->sock;
    evsrv* srv = self->srv;

    cwarn("[%d] <evsrv_conn_close>", sock);
    self->state = EVSRV_CONN_CLOSING;
    evsrv_conn_stop(self);
    if (self->srv->on_conn_close) {
        self->srv->on_conn_close(self, err);
    } else {
        if (!srv->on_conn_create) { // Then we created conn by ourselves, need to cleanup
            evsrv_conn_clean(self);
            free(self->rbuf);
            self->rbuf = NULL;
            free(self);
        }
    }

    if (sock > 0) {
        srv->connections[sock] = NULL;
    }
    --srv->active_connections;
    cwarn("[%d] </evsrv_conn_close>", sock);
}

void evsrv_write(evsrv_conn* conn, const char* buf, size_t len) {
    if ( len == 0 ) len = strlen(buf);

    if (conn->wuse) {
        //cwarn("have wbuf, use it");
        if (conn->wuse == conn->wlen) {
            conn->wlen += 2;
            conn->wbuf = realloc(conn->wbuf, sizeof(struct iovec) * ( conn->wlen ));
        }
        conn->wbuf[conn->wuse].iov_base = memdup(buf, len);
        conn->wbuf[conn->wuse].iov_len  = len;
        //cwarn("iov[%d] stored %zu: %p",conn->wuse,len, conn->wbuf[conn->wuse].iov_base);
        conn->wuse++;
        return;
    }

    ssize_t wr = 0;

    if (conn->wnow) {
        again:
            wr = write(conn->ww.fd, buf, len);
            // cwarn("writing %d",len);
            if ( wr == len ) {
                // success
                // cwarn("written now %zu %u",wr, *((uint32_t *)(buf + 8)) );
                return;
            }
            else
            if (wr > -1) {
                // cwarn("written part %zu %u",wr, *((uint32_t *)(buf + 8)) );
                //partial write, passthru
            }
            else
            {
                switch(errno) {
                    case EINTR:
                        goto again;
                    case EAGAIN:
                        wr = 0;
                        break;
                    default:
                        cerror("connection failed while write [now]");
                        evsrv_conn_close(conn, errno);
                        return;
                }
            }
    }

    conn->wlen = 2;
    conn->wbuf = calloc(conn->wlen, sizeof(struct iovec));
    conn->wbuf[0].iov_base = memdup(buf + wr, len - wr);
    conn->wbuf[0].iov_len = len - wr;
    conn->wuse = 1;

    ev_io_start(conn->srv->loop, &conn->ww);
    if (unlikely(conn->srv->write_timeout > 0)) {
        ev_timer_again(conn->srv->loop, &conn->tww);
    }
}


void _evsrv_conn_read_cb(struct ev_loop* loop, ev_io* w, int revents) {
    if (EV_ERROR & revents) {
        cerror("error occured");
        return;
    }

    evsrv_conn* self = SELFby(w, evsrv_conn, rw);

    evsrv_stop_timer(loop, &self->trw);

    ssize_t nread;
    again:
        nread = read(w->fd, self->rbuf + self->ruse, self->rlen - self->ruse);
        if (nread > 0) {
            self->ruse += nread;

            if (self->on_read) {
                self->on_read(self, nread);
            }
            if (self->ruse != 0 &&  self->ruse == self->rlen) {
                evsrv_conn_close(self, ENOBUFS);
            }

        } else if (nread < 0) {
            switch (errno) {
                case EAGAIN:
                    return;
                case EINTR:
                    goto again;
                default:
                    cerror("read error");
                    evsrv_conn_close(self, errno);
                    return;
            }
        } else {
            // cerror("read EOF");
            if (self->on_read) {
                self->on_read(self, 0);
            }
            evsrv_conn_close(self, errno);
        }
}

void _evsrv_conn_read_timeout_cb(struct ev_loop* loop, ev_timer* w, int revents) {
    if (EV_ERROR & revents) {
        cerror("error occured");
        return;
    }
    evsrv_conn* self = SELFby(w, evsrv_conn, trw);

    ev_timer_stop(loop, &self->trw);
    cwarn("read timer triggered");
    evsrv_conn_shutdown(self, EVSRV_SHUT_RDWR);
    evsrv_conn_close(self, errno);
}

void _evsrv_conn_write_cb(struct ev_loop* loop, ev_io* w, int revents) {
    if (EV_ERROR & revents) {
        cerror("error occured");
        return;
    }
    evsrv_conn* self = SELFby(w, evsrv_conn, ww);

    evsrv_stop_timer(loop, &self->tww);

    ssize_t wr;
    int iovcur, iov_total = 0;
    struct iovec *iov;

    //cwarn("on ww io %p -> %p (fd: %d) [ wbufs: %d of %d ]", w, self, w->fd, self->wuse, self->wlen);

    struct iovec *head_ptr = self->wbuf;

    again: {

        int iovs_to_write = self->wuse >= IOV_MAX ? IOV_MAX : self->wuse;

        // cwarn("wr = %d from iov = %p", iovs_to_write, head_ptr);

        // DEBUG1
        // wr = writev(w->fd, head_ptr, iovs_to_write > 1 ? iovs_to_write -1 : 1);

        // DEBUG2
        // if (head_ptr[iovs_to_write-1].iov_len > 1) {
        // 	head_ptr[iovs_to_write-1].iov_len--;
        // 	wr = writev(w->fd, head_ptr, iovs_to_write);
        // 	head_ptr[iovs_to_write-1].iov_len++;
        // } else {
        // 	wr = writev(w->fd, head_ptr, iovs_to_write);
        // }

        wr = writev(w->fd, head_ptr, iovs_to_write);
        if (wr > -1) {
            for (iovcur = 0; iovcur < iovs_to_write; iovcur++) {
                iov = &(head_ptr[iovcur]);
                if (wr < iov->iov_len) {
                    memmove(iov->iov_base, iov->iov_base + wr, iov->iov_len - wr);
                    iov->iov_len -= wr;
                    break;
                } else {
                    free(iov->iov_base);
                    wr -= iov->iov_len;
                }
            }
            // cwarn("wr = %d, iovcur = %d/%d of %d",wr, iovcur, iovs_to_write, self->wuse);
            if (iovcur == iovs_to_write && wr == 0) {
                if (iovs_to_write == self->wuse) {
                    // cwarn("all done");
                    self->wuse -= iovs_to_write;
                    ev_io_stop(loop, w);
                    return;
                } else {
                    // cwarn("again");
                    self->wuse -= iovcur;
                    head_ptr   += iovcur;
                    iov_total  += iovcur;
                    goto again;
                }
            } else {
                // written partially, finish
                self->wuse -= iov_total + iovcur;
                // cwarn("partially %p -> %p / %d", self->wbuf + iov_total + iovcur, self->wbuf, iov_total + iovcur);
                memmove(self->wbuf, self->wbuf + iov_total + iovcur, self->wuse * sizeof(struct iovec));
                if (unlikely(self->srv->write_timeout > 0)) {
                    ev_timer_again(loop, &self->tww); // written not all, so restart timer
                }
                return;
            }

        }
        else {
            switch(errno) {
                case EINTR:
                    goto again;
                case EAGAIN:
                    if (unlikely(self->srv->write_timeout > 0)) {
                        ev_timer_again(loop, &self->tww);
                    }
                    return;
                case EINVAL:
                    // einval may be a result only of corruption. dump a core is better than hangover
                    abort();
                default:
                    cerror("connection failed while write [io]");
                    evsrv_conn_close(self, errno);
                    return;
            }
        }
    }

}

void _evsrv_conn_write_timeout_cb(struct ev_loop* loop, ev_timer* w, int revents) {
    if (EV_ERROR & revents) {
        cerror("error occured");
        return;
    }
    evsrv_conn* self = SELFby(w, evsrv_conn, tww);

    ev_timer_stop(loop, &self->tww);
    cwarn("write timer triggered");
    evsrv_conn_shutdown(self, EVSRV_SHUT_RDWR);
    evsrv_conn_close(self, errno);
}


