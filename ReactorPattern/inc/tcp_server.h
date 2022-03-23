#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

typedef int (*tcp_server_connection_completed_t)();
typedef int (*tcp_server_receive_message_t)();
typedef int (*tcp_server_send_message_completed_t)();
typedef int (*tcp_server_connection_closed_t)();

struct tcp_server {
	struct acceptor *acceptor;
	struct thread_pool *pool;
};

#endif	/* #ifndef __TCP_SERVER_H__ */
