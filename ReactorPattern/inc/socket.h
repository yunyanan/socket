#ifndef __SOCKET_H__
#define __SOCKET_H__

struct socketops {
	int (*create_nonblocking)(int domain, int type);

	void *data;
};

struct socketops *socketops_init(void);
void socketops_free(struct socketops *ops);

#endif	/* #ifndef __SOCKET_H__ */
