#ifndef __LOG_H__
#define __LOG_H__

#include <stdint.h>

#define LOG_ERRNO				__LINE__

#define LOG_LVL_ERROR			1
#define LOG_LVL_WARNG			2
#define LOG_LVL_DEBUG			3
#define LOG_LVL_RUNIG			4
#define LOG_LVL_ENDMARK			5

#ifndef LOG_LVL_ISUSED
#define LOG_LVL_ISUSED			LOG_LVL_RUNIG
#endif

extern uint8_t log_lvl;
void log_printf(uint8_t lvl, const char *func, int line, const char *format, ...);

static __inline__ int log_isact(uint8_t lvl)
{
	if (lvl > log_lvl) {
		return -LOG_ERRNO;
	}

	return 0;
}

#define LOG(_lvl, ...)											\
	do {														\
		if (log_isact(_lvl) == 0) {								\
			log_printf(_lvl, __func__, __LINE__, __VA_ARGS__);	\
		}														\
	} while (0);

#define LOGERR(...)								\
	LOG(LOG_LVL_ERROR,  __VA_ARGS__)
#define LOGWAN(...)								\
	LOG(LOG_LVL_WARNG,  __VA_ARGS__)
#define LOGDBG(...)								\
	LOG(LOG_LVL_DEBUG,  __VA_ARGS__)
#define LOGRUN(...)								\
	LOG(LOG_LVL_RUNIG,  __VA_ARGS__)

#endif	/* #ifndef __LOG_H__ */
