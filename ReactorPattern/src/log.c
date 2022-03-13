#include <stdio.h>
#include <stdarg.h>

#include "log.h"

uint8_t log_lvl = LOG_LVL_ISUSED;
static const char *log_lvlstr[LOG_LVL_ENDMARK] = {
	[LOG_LVL_ERROR] = "ERROR",
	[LOG_LVL_WARNG] = "WARNG",
	[LOG_LVL_DEBUG] = "DEBUG",
	[LOG_LVL_RUNIG] = "RUNIG",
};

static __inline__ int log_output(const char *msg)
{
	return printf("%s\n", msg);
}

void log_printf(uint8_t lvl, const char *func, int line, const char *format, ...)
{
	char buff[1024];
	va_list args;
	int len;

	len = snprintf(buff, sizeof(buff), "[%5s] %24s:%04d ", log_lvlstr[lvl], func, line);

	va_start(args, format);
	vsnprintf(&buff[len], sizeof(buff) - len, format, args);
	va_end(args);

	 log_output(buff);
 }
