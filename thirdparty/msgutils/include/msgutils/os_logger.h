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

#ifndef __OS_LOGGER_H__
#define __OS_LOGGER_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS_LOG_COLOR_ENABLE

enum os_logprio {
    OS_LOG_PRIO_FATAL = 0,
    OS_LOG_PRIO_ERROR,
    OS_LOG_PRIO_WARNING,
    OS_LOG_PRIO_INFO,
    OS_LOG_PRIO_DEBUG,
    OS_LOG_PRIO_VERBOSE,
};

void os_logger_config(bool enable, enum os_logprio prio);

// [date] [time] [pid] [tid] [prio] [tag]:[func]:[line]: [log]
void os_logger_trace(enum os_logprio prio, const char *tag,
                     const char *func, unsigned int line,
                     const char *format, ...);

#if defined(OS_LOG_COLOR_ENABLE)
    #define OS_LOG_BLACK         "\033[0;30m"
    #define OS_LOG_RED           "\033[0;31m"
    #define OS_LOG_GREEN         "\033[0;32m"
    #define OS_LOG_BROWN         "\033[0;33m"
    #define OS_LOG_BLUE          "\033[0;34m"
    #define OS_LOG_PURPLE        "\033[0;35m"
    #define OS_LOG_CYAN          "\033[0;36m"
    #define OS_LOG_GRAY          "\033[1;30m"
    #define OS_LOG_COLOR_RESET   "\033[0m"
    #define OS_LOG_COLOR_F       OS_LOG_RED
    #define OS_LOG_COLOR_E       OS_LOG_RED
    #define OS_LOG_COLOR_W       OS_LOG_BROWN
    #define OS_LOG_COLOR_I       OS_LOG_GREEN
    #define OS_LOG_COLOR_D       OS_LOG_BLUE
    #define OS_LOG_COLOR_V       "\033[1;30m"
#else
    #define OS_LOG_COLOR_RESET
    #define OS_LOG_COLOR_F
    #define OS_LOG_COLOR_E
    #define OS_LOG_COLOR_W
    #define OS_LOG_COLOR_I
    #define OS_LOG_COLOR_D
    #define OS_LOG_COLOR_V
#endif

#define OS_LOG_FORMAT(letter, format)  OS_LOG_COLOR_ ## letter format OS_LOG_COLOR_RESET

#define OS_LOGF(tag, format, ...) \
    os_logger_trace(OS_LOG_PRIO_FATAL, tag, __FUNCTION__, __LINE__, OS_LOG_FORMAT(F, format), ##__VA_ARGS__)

#define OS_LOGE(tag, format, ...) \
    os_logger_trace(OS_LOG_PRIO_ERROR, tag, __FUNCTION__, __LINE__, OS_LOG_FORMAT(E, format), ##__VA_ARGS__)

#define OS_LOGW(tag, format, ...) \
    os_logger_trace(OS_LOG_PRIO_WARNING, tag, __FUNCTION__, __LINE__, OS_LOG_FORMAT(W, format), ##__VA_ARGS__)

#define OS_LOGI(tag, format, ...) \
    os_logger_trace(OS_LOG_PRIO_INFO, tag, __FUNCTION__, __LINE__, OS_LOG_FORMAT(I, format), ##__VA_ARGS__)

#define OS_LOGD(tag, format, ...) \
    os_logger_trace(OS_LOG_PRIO_DEBUG, tag, __FUNCTION__, __LINE__, OS_LOG_FORMAT(D, format), ##__VA_ARGS__)

#define OS_LOGV(tag, format, ...) \
    os_logger_trace(OS_LOG_PRIO_VERBOSE, tag, __FUNCTION__, __LINE__, OS_LOG_FORMAT(V, format), ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __OS_LOGGER_H__ */
