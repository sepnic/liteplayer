/* The MIT License (MIT)
 *
 * Copyright (c) 2019 luoyun <sysu.zqlong@gmail.com>
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

#ifndef __SW_TIMER_H__
#define __SW_TIMER_H__

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct swtimer *swtimer_t;

struct swtimer_attr {
    const char *name;        // name is assigned to the timer, purely to assist debugging
    unsigned long period_ms; // the timer period in milliseconds
    bool reload;             // if reload set to true then the timer will expire repeatedly
                             // if reload set to false then the timer will be a one-shot timer
};

swtimer_t swtimer_create(struct swtimer_attr *attr, void (*swtimer_callback)());

int swtimer_start(swtimer_t timer);

int swtimer_stop(swtimer_t timer);

bool swtimer_is_active(swtimer_t timer);

void swtimer_destroy(swtimer_t timer);

#ifdef __cplusplus
}
#endif

#endif /* __SW_TIMER_H__ */
