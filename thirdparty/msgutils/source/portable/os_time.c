/* The MIT License (MIT)
 *
 * Copyright (c) 2018 luoyun <sysu.zqlong@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>
#include <unistd.h>
#include <time.h>

#include "msgutils/os_time.h"

unsigned long OS_TIMESTAMP_TO_UTC(struct os_realtime *rt)
{
    struct tm now;
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    gmtime_r(&ts.tv_sec, &now);

    if (rt != NULL) {
        rt->year = now.tm_year + 1900;
        rt->mon  = now.tm_mon + 1;
        rt->day  = now.tm_mday;
        rt->hour = now.tm_hour;
        rt->min  = now.tm_min;
        rt->sec  = now.tm_sec;
        rt->msec = (ts.tv_nsec / 1000000) % 1000;
    }
    return ts.tv_sec;
}

unsigned long OS_TIMESTAMP_TO_LOCAL(struct os_realtime *rt)
{
    struct tm now;
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &now);

    if (rt != NULL) {
        rt->year = now.tm_year + 1900;
        rt->mon  = now.tm_mon + 1;
        rt->day  = now.tm_mday;
        rt->hour = now.tm_hour;
        rt->min  = now.tm_min;
        rt->sec  = now.tm_sec;
        rt->msec = (ts.tv_nsec / 1000000) % 1000;
    }
    return ts.tv_sec;
}

unsigned long long OS_MONOTONIC_USEC()
{
    struct timespec ts;
    unsigned long long cputime;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    cputime = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return cputime;
}

unsigned long long OS_REALTIME_USEC()
{
    struct timespec ts;
    unsigned long long cputime;

    clock_gettime(CLOCK_REALTIME, &ts);

    cputime = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return cputime;
}
