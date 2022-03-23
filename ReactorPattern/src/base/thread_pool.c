#include "log.h"

void *thread_pool_subthread_entry(void *arg)
{
	struct thread_info *info = (struct thread_info *)arg;
	char name[16];

	info->pid = pthread_self();
	snprintf(name, 16, "sub-thread:%d", info->pid);

	info->eloop = event_loop_init(name);

	event_loop_run(info->eloop);
}

struct thread_pool *thread_pool_new(int num)
{
	struct thread_pool *pool = NULL;
	int size;

	size = sizeof(struct thread_pool);
	/* 创建 thread pool 对象 */
	pool = malloc(size);
	if (!pool) {
		LOGERR("get thread pool memory failed");
		return NULL;
	}
	memset(pool, 0x00, size);

	size = sizeof(struct thread_info) * num;
	pool->subinfo = malloc(size);
	if (!pool->subinfo) {
		LOGERR("get sub thread info memory failed");
		free(pool);
		return NULL;
	}
	memset(pool, 0x00, size);

	/* 同步信息 */
	pool->count = num;

	return pool;
}

void thread_pool_start(struct thread_pool *pool)
{
	int i, ret;
	int pid;

	/* 创建线程 */
	for (i=0; i<pool->count; i++) {
		ret = pthread_create(&pid, NULL, thread_pool_subthread_entry, &pool->subinfo[i]);
		if (ret == 0) {
			pthread_detach(pid);
			pool->valid ++;
		}
	}
}

void thread_pool_destroy(struct thread_pool *pool)
{
	if (pool) {
		free(pool);
	}
}
