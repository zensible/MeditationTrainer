#ifndef __NSUTIL_H
#define __NSUTIL_H

#include <time.h>
#include "pctimer.h"

#define MAXLEN 16384

int rprintf(const char *fmt, ...);
int rexit(int errcode);
extern int max_fd;
void updateMaxFd(int fd);
void rtime(time_t *t);
void rsleep(int ms);

#ifdef __MINGW32__
#define OSTYPESTR "Windows"
#else
#define OSTYPESTR "Linux"
#endif

#endif
