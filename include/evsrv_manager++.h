#ifndef LIBEVSERVER_EVSRV_MANAGER_PP_H
#define LIBEVSERVER_EVSRV_MANAGER_PP_H

#ifndef __cplusplus
#error evsrv_manager++.h is designed only for c++
#endif

#include <ev++.h>
#include "evsrv_manager.h"

namespace ev {
    class srv;

    class srv_manager : public evsrv_manager {
        friend class srv;
    public:
        srv_manager(ev::loop_ref loop, evsrv_info* servers, size_t servers_count) {
            evsrv_manager_init(loop.raw_loop, static_cast<evsrv_manager*>(this), servers, servers_count);
        }

        virtual ~srv_manager() {
            evsrv_manager_destroy(static_cast<evsrv_manager*>(this));
        }

        template <class K, void (K::*method)(srv_manager& s)>
        void set_on_started(K* object) {
            _set_on_started(object, _on_started_method_thunk<K, method>);
        }

        template <void (*function)(srv_manager& s)>
        void set_on_started(void* data = NULL) {
            _set_on_started(data, _on_started_function_thunk < function > );
        }

        virtual void bind() { evsrv_manager_bind(static_cast<evsrv_manager*>(this)); }
        virtual void listen() { evsrv_manager_listen(static_cast<evsrv_manager*>(this)); }
        virtual void accept() { evsrv_manager_accept(static_cast<evsrv_manager*>(this)); }
        virtual void start() {
            bind();
            listen();
            accept();
        }
        virtual void stop() {
            evsrv_manager_stop(static_cast<evsrv_manager*>(this));
        }

        template <class K, void (K::*method)(srv_manager&)>
        void graceful_stop(K* object) {
            this->data = object;
            evsrv_manager_graceful_stop(static_cast<evsrv_manager*>(this), _on_graceful_stop_method_thunk<K, method>);
        }

        template <void (*function)(srv_manager&)>
        void graceful_stop() {
            evsrv_manager_graceful_stop(static_cast<evsrv_manager*>(this), _on_graceful_stop_function_thunk<function>);
        }

    private:
        void* data;

        void _set_on_started(const void* data, evsrv_manager_on_started_cb cb) {
            this->data = (void*) data;
            evsrv_manager_set_on_started(static_cast<evsrv_manager*>(this), cb);
        }

        template <class K, void (K::*method)(srv_manager& s)>
        static void _on_started_method_thunk(evsrv_manager* s) {
            srv_manager* m = static_cast<srv_manager*>(s);
            (static_cast<K*>(m->data)->*method)(*m);
        }

        template <void (*function)(srv_manager& s)>
        static void _on_started_function_thunk(evsrv_manager* s) {
            function(*static_cast<srv_manager*>(s));
        }


        template <class K, void (K::*method)(srv_manager&)>
        static void _on_graceful_stop_method_thunk(evsrv_manager* s) {
            srv_manager* m = static_cast<srv_manager*>(s);
            (static_cast<K*>(m->data)->*method)(*m);
        }

        template <void (*function)(srv_manager&)>
        static void _on_graceful_stop_function_thunk(evsrv_manager* s) {
            function(*static_cast<srv_manager*>(s));
        }
    };
}

#endif //LIBEVSERVER_EVSRV_MANAGER_PP_H
