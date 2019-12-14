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

#ifndef __OS_TIME_H__
#define __OS_TIME_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct os_realtime {
    int year;
    int mon;
    int day;
    int hour;
    int min;
    int sec;
    int msec;
};

// utc clock_time, return unix timestamp
unsigned long OS_TIMESTAMP_TO_UTC(struct os_realtime *rt);
// local clock_time, return unix timestamp
unsigned long OS_TIMESTAMP_TO_LOCAL(struct os_realtime *rt);

// monotonictime: clock that cannot be set and represents monotonic time since system bootup
unsigned long long OS_MONOTONIC_USEC();
// realtime: system-wide clock that measures real time (utc timestamp) since 1970.1.1-00:00:00
unsigned long long OS_REALTIME_USEC();

#ifdef __cplusplus
}
#endif

#endif /* __OS_TIME_H__ */
