#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include "nsutil.h"

#ifdef __MINGW32__
#include <windows.h>
#else
#include <unistd.h>
#endif

int max_fd = 0;

void rtime(time_t *t)
{
	time(t);
}

void updateMaxFd(int fd)
{
	if (max_fd < fd + 1)
		max_fd = fd + 1;
}

int rprintf(const char *fmt, ...)
{
	int retval;
	char buf[4096];
	va_list va;
	buf[4095] = '\0';
	va_start(va, fmt);
	vsnprintf(buf, 4095, fmt, va);
	va_end(va);
	retval = fprintf(stdout, "%s", buf);
	fflush(stdout);
	return retval;
}

int rexit(int errcode) {
	printf("Exitting with error code %d\n", errcode);
	fflush(stdout);
	*((char *) 0) = 0;
	assert(0);
	return 0;
}
void rsleep(int ms)
{
#ifdef __MINGW32__
	Sleep(ms);
#else
	usleep(ms*10000);
#endif
}
