#include "log.h"


struct tcp_server *tcp_server_init(int port, int maxcli, tcp_server_connection_completed conn_comp,
								   tcp_server_receive_message recv_msg, tcp_server_send_message_completed send_comp,
								   tcp_server_connection_closed conn_closed)
{
	struct tcp_server *server = NULL;
	struct accetpor *acceptor = NULL;
	struct thread_pool *pool = NULL;

	server = malloc(sizeof(struct tcp_server));
	if (!server) {
		LOGERR("get tcp server memory failed");
		return NULL;
	}

	acceptor = acceptor_init(port);
	if (!acceptor) {
		LOOGERR("acceptor init failed");
		goto label_tcp_server_init;
	}

	/* 创建 thread pool */
	pool = thread_pool_new(maxcli);
	if (!pool) {
		LOOGERR("thread pool create failed");
		goto label_tcp_server_init;
	}

	server->acceptor = acceptor;
	server->pool = pool;

	return server;
label_tcp_server_init:
	if (pool) {
		thread_pool_destroy(pool);
	}
	if (acceptor) {
		acceptor_destroy(acceptor);
	}
	if (server) {
		free(server);
	}
	return NULL;
}

void tcp_server_start(struct tcp_server *server)
{
	/* 启动 thread pool */
	thread_pool_start(server->pool);

	/* todo: channel new */
	/* todo: add channel event */
}
