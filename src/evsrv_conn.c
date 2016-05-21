#include <unistd.h>
#include <stdlib.h>
#include "evsrv_conn.h"
#include "util.h"
#include "evsrv.h"

static void _evsrv_conn_read_cb(struct ev_loop* loop, ev_io* w, int revents);
static void _evsrv_conn_read_timeout_cb(struct ev_loop* loop, ev_timer* w, int revents);

static void _evsrv_conn_write_cb(struct ev_loop* loop, ev_io* w, int revents);
static void _evsrv_conn_write_timeout_cb(struct ev_loop* loop, ev_timer* w, int revents);

/*************************** evsrv_conn ***************************/

void evsrv_conn_init(evsrv_conn* self, evsrv* srv, struct evsrv_conn_info* info) {
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

    self->data = NULL;

    if (evsrv_socket_set_nonblock(self->info->sock) < 0) {
        cerror("Error setting socket %d to nonblock", self->info->sock);
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
    if (self->srv->read_timeout > 0) {
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

void evsrv_conn_read_timer_again(evsrv_conn* self) {
    if (self->srv->read_timeout > 0) {
        ev_timer_again(self->srv->loop, &self->trw);
    }
}

void evsrv_conn_read_timer_stop(evsrv_conn* self) {
    evsrv_stop_timer(self->srv->loop, &self->trw);
}

void evsrv_conn_destroy(evsrv_conn* self) {
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
    enum evsrv_conn_state prev_state = self->state;

    self->state = EVSRV_CONN_CLOSING;
    evsrv_conn_stop(self);
    if (self->srv->on_conn_destroy) {
        self->srv->on_conn_destroy(self, err);
    } else {
        if (!srv->on_conn_create) { // Then we created conn by ourselves, need to cleanup
            evsrv_conn_destroy(self);
            free(self->rbuf);
            self->rbuf = NULL;
            free(self);
        }
    }

    if (sock > 0) {
        srv->connections[sock] = NULL;
    }
    --srv->active_connections;

    if (prev_state == EVSRV_CONN_PENDING_CLOSE &&
        srv->active_connections == 0 &&
        srv->state == EVSRV_GRACEFULLY_STOPPING) {

        srv->state = EVSRV_STOPPED;
        srv->on_graceful_stop(srv);
    }
}

void evsrv_conn_write(evsrv_conn* conn, const void* buffer, size_t len) {
    const char* buf = (const char*) buffer;
    if (len == 0) len = strlen(buf);

    if (conn->wuse) {
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

    if (unlikely(conn->wlen == 0)) {
        conn->wlen = 2;
        conn->wbuf = calloc(conn->wlen, sizeof(struct iovec));
    }
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
            evsrv_conn_shutdown(self, EVSRV_SHUT_RDWR);
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
        evsrv_conn_shutdown(self, EVSRV_SHUT_RDWR);
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

        size_t iovs_to_write = self->wuse >= IOV_MAX ? IOV_MAX : self->wuse;

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

        wr = writev(w->fd, head_ptr, (int) iovs_to_write);
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