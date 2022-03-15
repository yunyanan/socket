#ifndef __SOCKET_H__
#define __SOCKET_H__

struct socketops {
	int (*create_nonblocking)(struct acceptor *acceptor, int domain, int type);
	void (*set_reuseaddr)(struct acceptor *acceptor, int on);
	void (*set_reuseport)(struct acceptor *acceptor, int on);
	int (*bind_addr)(struct acceptor *acceptor, int port);
	int (*listen)(struct acceptor *acceptor);
	void *data;
};

struct socketops *socketops_init(void);
void socketops_free(struct socketops *ops);

#endif	/* #ifndef __SOCKET_H__ */
