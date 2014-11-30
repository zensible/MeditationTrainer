/* Win32 performance counter functions to get a high-resolution timer
 *
 * By Wu Yongwei
 *
 */

#include <stdio.h>
#include <pctimer.h>

#ifdef __MINGW32__
#include <windows.h>
#include <winsock2.h>
/* Broken version just for windows */
int gettimeofday(struct timeval *tv, void *tz);
#else
#define HAVE_GETTIMEOFDAY
#endif

#ifndef __GNUC__
#define EPOCHFILETIME (116444736000000000i64)
#else
#define EPOCHFILETIME (116444736000000000LL)
#endif


#include <sys/time.h>


pctimer_t pctimer(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000;
}

/*
 * Defines gettimeofday
 *
 * Based on timeval.h by Wu Yongwei
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef HAVE_GETTIMEOFDAY
int gettimeofday(struct timeval *tv, void *tz)
{
    FILETIME        ft;
    LARGE_INTEGER   li;
    __int64         t;

    if (tv)
    {
        GetSystemTimeAsFileTime(&ft);
        li.LowPart  = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        t  = li.QuadPart;       /* In 100-nanosecond intervals */
        t -= EPOCHFILETIME;     /* Offset to the Epoch time */
        t /= 10;                /* In microseconds */
        tv->tv_sec  = (long)(t / 1000000);
        tv->tv_usec = (long)(t % 1000000);
    }
    return 0;
}

#endif /* HAVE_GETTIMEOFDAY */

