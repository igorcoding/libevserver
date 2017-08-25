#ifndef LIBEVSERVER_EVSRV_CONN_PP_H
#define LIBEVSERVER_EVSRV_CONN_PP_H

#ifndef __cplusplus
#error evsrv_conn++.h is designed only for c++
#endif

#include "evsrv_conn.h"
#include "evsrv++.h"

namespace ev {
    class srv_conn : private evsrv_conn {
        friend class srv;
    public:
        srv_conn(ev::srv& s, evsrv_conn_info* info) {
            evsrv_conn_init(this, static_cast<evsrv*>(&s), info);
        }

        virtual ~srv_conn() {
            clean();
        }


        ev::srv& srv() { return *static_cast<ev::srv*>(evsrv_conn::srv); }
        evsrv_conn_info& conn_info() const { return *evsrv_conn::info; }

        char* rbuf() const { return evsrv_conn::rbuf; }
        size_t& ruse() { return evsrv_conn::ruse; }
        size_t rlen() const { return evsrv_conn::rlen; }

        iovec* wbuf() const { return evsrv_conn::wbuf; }
        size_t wuse() const { return evsrv_conn::wuse; }
        size_t wlen() const { return evsrv_conn::wlen; }
        bool write_now() const { return evsrv_conn::wnow; }

        virtual void set_rbuf(char* buf, size_t len) {
            evsrv_conn_set_rbuf(static_cast<evsrv_conn*>(this), buf, len);
        }
        void set_write_now(bool write_now) { evsrv_conn::wnow = write_now; }

        template <class K, void (K::*method)(srv_conn&, ssize_t)>
        void set_on_read(K* object) {
            _set_on_read(_on_read_method_thunk<K, method>, object);
        }

        template <void (*function)(srv_conn&, ssize_t)>
        void set_on_read() {
            _set_on_read(_on_read_function_thunk<function>, NULL);
        }

        template <class K, void (K::*method)(srv_conn&)>
        void set_on_graceful_close(K* object) {
            _set_on_graceful_close(_on_graceful_close_method_thunk<K, method>, object);
        }

        template <void (*function)(srv_conn&)>
        void set_on_graceful_close() {
            _set_on_graceful_close(_on_graceful_close_function_thunk<function>, NULL);
        }


        virtual void start() {
            evsrv_conn_start(this);
        }

        virtual void stop() {
            evsrv_conn_stop(this);
        }

        virtual void clean() {
            evsrv_conn_destroy(this);
        }

        virtual void shutdown(int how = EVSRV_SHUT_RDWR) {
            evsrv_conn_shutdown(this, how);
        }

        virtual void close(int err = 0) {
            evsrv_conn_close(this, err);
        }

        virtual void write(const void* buffer, size_t len = 0) {
            evsrv_conn_write(this, buffer, len);
        }

        void read_timer_stop() {
            evsrv_conn_read_timer_stop(this);
        }

        void read_timer_again() {
            evsrv_conn_read_timer_again(this);
        }

    private:

        void _set_on_read(evsrv_on_read_cb cb, const void* data = NULL) {
            evsrv_conn_set_on_read(static_cast<evsrv_conn*>(this), cb);
            this->data = (void*) data;
        }

        template <class K, void (K::*method)(srv_conn&, ssize_t)>
        static void _on_read_method_thunk(evsrv_conn* c, ssize_t size) {
            (static_cast<K*>(c->data)->*method)(*static_cast<srv_conn*>(c), size);
        }

        template <void (*function)(srv_conn&, ssize_t)>
        static void _on_read_function_thunk(evsrv_conn* c, ssize_t size) {
            function(*static_cast<srv_conn*>(c), size);
        }


        void _set_on_graceful_close(evsrv_conn_on_graceful_close_cb cb, const void* data = NULL) {
            evsrv_conn_set_on_graceful_close(static_cast<evsrv_conn*>(this), cb);
            this->data = (void*) data;
        }

        template <class K, void (K::*method)(srv_conn&)>
        static void _on_graceful_close_method_thunk(evsrv_conn* c) {
            (static_cast<K*>(c->data)->*method)(*static_cast<srv_conn*>(c));
        }

        template <void (*function)(srv_conn&)>
        static void _on_graceful_close_function_thunk(evsrv_conn* c) {
            function(*static_cast<srv_conn*>(c));
        }

    };
}

#endif //LIBEVSERVER_EVSRV_CONN_PP_H
