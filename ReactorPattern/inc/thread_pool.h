#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

struct thread_info {
	pthread_t pid;
	struct event_loop *eloop;
};

struct thread_pool {
	int max_count;
	int valid_count;
	struct thread_info *subinfo;
};

#endif	/* #ifndef __THREAD_POOL_H__ */
