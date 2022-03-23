#include <stdio.h>

#include "log.h"
#include "event_loop.h"

#define SERVER_ERRNO				__LINE__

int server_connection_completed()
{
	return 0;
}

int tcp_server_receive_message()
{
	return 0;
}

int tcp_server_send_message_completed()
{
	return 0;
}

int tcp_server_connection_closed()
{
	return 0;
}

int main(int argc, char **argv)
{
	LOG(LOG_LVL_ERROR, "Log message");
	LOGERR("Error message %d",	 LOG_LVL_ERROR);
	LOGWAN("Warning message %d", LOG_LVL_WARNG);
	LOGDBG("Debug message %d",	 LOG_LVL_DEBUG);
	LOGRUN("Running message %d", LOG_LVL_RUNIG);

	/* todo: 创建 event loop */
	struct event_loop *eloop = event_loop_init("server");
	if (!eloop) {
		LOGERR("event loop init failed");
		return -SERVER_ERRNO;
	}

	LOGRUN("loop name: %s", eloop->name);

	/* 创建 tcp 服务器对象 */
	struct tcp_server *server = tcp_server_init(8999, 10, server_connection_completed, tcp_server_receive_message,
												tcp_server_send_message_completed,  tcp_server_connection_closed);
	/* 启动 tcp 服务器 */
	tcp_server_start(server);

	/* todo： event loop run */
	event_loop_run(eloop);

	return 0;
}
