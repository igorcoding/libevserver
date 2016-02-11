#ifndef LIBEVSERVER_EVSRV_PP_H
#define LIBEVSERVER_EVSRV_PP_H

#ifndef __cplusplus
#error evsrv++.h is designed only for c++
#endif

#include <functional>
#include <ev++.h>
#include "evsrv.h"
#include "evsrv_manager++.h"

namespace ev {
    class srv_conn;
    class srv : public evsrv {
    public:
        srv(ev::loop_ref& loop, const char* host, const char* port) {
            evsrv_init(loop.raw_loop, this, host, port);
        }

        ~srv() {
            evsrv_destroy(this);
        }

        srv_manager* manager() { return static_cast<srv_manager*>(evsrv::manager); }
        size_t id() const { return evsrv::id; }
        const char* host() const { return evsrv::host; }
        const char* port() const { return evsrv::port; }
        const evsrv_sockaddr& sockaddr() const { return evsrv::sockaddr; }

        int sock() const { return evsrv::sock; }
        int backlog() const { return evsrv::backlog; }
        double read_timeout() const { return evsrv::read_timeout; }
        double write_timeout() const { return evsrv::write_timeout; }

        int32_t active_connections() const { return evsrv::active_connections; }

//        void* data() const { return evsrv::data; }


        void set_backlog(int backlog = SOMAXCONN) { evsrv::backlog = backlog; }
        void set_read_timeout(double read_timeout) { evsrv::read_timeout = read_timeout; }
        void set_write_timeout(double write_timeout) { evsrv::write_timeout = write_timeout; }
//        void set_data(void* data) { evsrv::data = data; }

        template <class K, void (K::*method)(srv& s)>
        void set_on_started(K* object) {
            _set_on_started(object, _on_started_method_thunk<K, method>);
        }

        template <void (*function)(srv& s)>
        void set_on_started(void* data = NULL) {
            _set_on_started(data, _on_started_function_thunk < function > );
        }


        template <class K, void (K::*method)(srv_conn&, ssize_t)>
        void set_on_read(K* object) {
            _set_on_read(_on_read_method_thunk<K, method>, object);
        }

        template <void (*function)(srv_conn&, ssize_t)>
        void set_on_read() {
            _set_on_read(_on_read_function_thunk<function>, NULL);
        }


        template <class K, void (K::*method)(srv_conn&)>
        void set_on_conn_ready(K* object) {
            _set_on_conn_ready(_on_conn_ready_method_thunk<K, method>, object);
        }

        template <void (*function)(srv_conn&)>
        void set_on_conn_ready() {
            _set_on_conn_ready(_on_conn_ready_function_thunk<function>, NULL);
        }


        template <class K, srv_conn* (K::*on_create_method)(srv&, evsrv_conn_info*), void (K::*on_destroy_method)(srv_conn&, int)>
        void set_on_conn(K* object) {
            _set_on_conn(_on_conn_create_method_thunk<K, on_create_method>,
                         _on_conn_destroy_method_thunk<K, on_destroy_method>,
                         object);
        }

        template <srv_conn* (*on_create_function)(srv&, evsrv_conn_info*), void (*on_destroy_function)(srv_conn&, int)>
        void set_on_conn() {
            _set_on_conn(_on_conn_create_function_thunk<on_create_function>,
                         _on_conn_destroy_function_thunk<on_destroy_function>,
                         NULL);
        }



        int bind() { return evsrv_bind(static_cast<evsrv*>(this)); }
        int listen() { return evsrv_listen(static_cast<evsrv*>(this)); }
        int accept() { return evsrv_accept(static_cast<evsrv*>(this)); }
        void start() {
            if (bind() < 0) {
                // TODO
                return;
            }
            if (listen() < 0) {
                // TODO
                return;
            }
            if (accept() < 0) {
                // TODO
                return;
            }
        }
        void stop()  { evsrv_stop(static_cast<evsrv*>(this)); }

        template <class K, void (K::*method)(srv&)>
        void graceful_stop(K* object) {
            this->data = object;
            evsrv_graceful_stop(static_cast<evsrv*>(this), _on_graceful_stop_method_thunk<K, method>);
        }

        template <void (*function)(srv&)>
        void graceful_stop() {
            evsrv_graceful_stop(static_cast<evsrv*>(this), _on_graceful_stop_function_thunk<function>);
        }

    private:

        void _set_on_started(const void* data, evsrv_on_started_cb cb) {
            this->data = (void*) data;
            evsrv_set_on_started(static_cast<evsrv*>(this), cb);
        }

        template <class K, void (K::*method)(srv& s)>
        static void _on_started_method_thunk(evsrv* s) {
            (static_cast<K*>(s->data)->*method)(*static_cast<srv*>(s));
        }

        template <void (*function)(srv& s)>
        static void _on_started_function_thunk(evsrv* s) {
            function(*static_cast<srv*>(s));
        }


        void _set_on_read(evsrv_on_read_cb cb, const void* data = NULL) {
            evsrv_set_on_read(static_cast<evsrv*>(this), cb);
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


        void _set_on_conn_ready(evsrv_on_conn_ready_cb cb, const void* data = NULL) {
            evsrv_set_on_conn_ready(static_cast<evsrv*>(this), cb);
            this->data = (void*) data;
        }

        template <class K, void (K::*method)(srv_conn&)>
        static void _on_conn_ready_method_thunk(evsrv_conn* c) {
            (static_cast<K*>(c->data)->*method)(*static_cast<srv_conn*>(c));
        }

        template <void (*function)(srv_conn&)>
        static void _on_conn_ready_function_thunk(evsrv_conn* c) {
            function(*static_cast<srv_conn*>(c));
        }



        void _set_on_conn(evsrv_on_conn_create_cb create_cb, evsrv_on_conn_destroy_cb destroy_cb, const void* data = NULL) {
            evsrv_set_on_conn(static_cast<evsrv*>(this), create_cb, destroy_cb);
            this->data = (void*) data;
        }

        template <class K, srv_conn* (K::*method)(srv&, evsrv_conn_info*)>
        static evsrv_conn* _on_conn_create_method_thunk(evsrv* s, evsrv_conn_info* info) {
            srv_conn* c = (static_cast<K*>(s->data)->*method)(*static_cast<srv*>(s), info);
            return static_cast<evsrv_conn*>(c);
        }

        template <srv_conn* (*function)(srv&, evsrv_conn_info*)>
        static evsrv_conn* _on_conn_create_function_thunk(evsrv* s, evsrv_conn_info* info) {
            srv_conn* c = function(*static_cast<srv*>(s), info);
            return static_cast<evsrv_conn*>(c);
        }

        template <class K, void (K::*method)(srv_conn&, int)>
        static void _on_conn_destroy_method_thunk(evsrv_conn* c, int err) {
            (static_cast<K*>(c->data)->*method)(*static_cast<srv_conn*>(c), err);
        }

        template <void (*function)(srv_conn&, int)>
        static void _on_conn_destroy_function_thunk(evsrv_conn* c, int err) {
            function(*static_cast<srv_conn*>(c), err);
        }


        template <class K, void (K::*method)(srv&)>
        static void _on_graceful_stop_method_thunk(evsrv* s) {
            (static_cast<K*>(s->data)->*method)(*static_cast<srv*>(s));
        }

        template <void (*function)(srv&)>
        static void _on_graceful_stop_function_thunk(evsrv* s) {
            function(*static_cast<srv*>(s));
        }


    };
}

#endif //LIBEVSERVER_EVSRV_PP_H
