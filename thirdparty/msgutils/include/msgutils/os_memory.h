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

#ifndef __MEMORY_LEAK_DETECT_H__
#define __MEMORY_LEAK_DETECT_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define ENABLE_MEMORY_LEAK_DETECT
//#define ENABLE_MEMORY_OVERFLOW_DETECT

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(ENABLE_MEMORY_LEAK_DETECT) && !defined(ENABLE_MEMORY_OVERFLOW_DETECT)
    #define OS_MALLOC(size) malloc((size_t)(size))
    #define OS_CALLOC(n, size) calloc((size_t)(n), (size_t)(size))
    #define OS_REALLOC(ptr, size) realloc((void *)(ptr), (size_t)(size))
    #define OS_FREE(ptr) \
        do {\
             if (ptr) {\
                 free((void *)(ptr));\
                 (ptr) = NULL;\
             }\
         } while (0)
    #define OS_STRDUP(str) (str) ? strdup(str) : NULL
    #define OS_STREQUAL(str1, str2) (((str1) && (str2)) ? (strcmp((str1), (str2)) == 0) : false)
    #define OS_MEMORY_DUMP() do {} while (0)

#else
    void *memory_detect_malloc(size_t size, const char *file, const char *func, int line);
    void *memory_detect_calloc(size_t n, size_t size, const char *file, const char *func, int line);
    void *memory_detect_realloc(void *ptr, size_t size, const char *file, const char *func, int line);
    void memory_detect_free(void *ptr, const char *file, const char *func, int line);
    void *memory_detect_strdup(void *str, const char *file, const char *func, int line);
    void memory_detect_dump();

    #define OS_MALLOC(size) \
        memory_detect_malloc((size_t)(size), __FILE__, __FUNCTION__, __LINE__)
    #define OS_CALLOC(n, size) \
        memory_detect_calloc((size_t)(n), (size_t)(size), __FILE__, __FUNCTION__, __LINE__)
    #define OS_REALLOC(ptr, size) \
        memory_detect_realloc((void *)(ptr), (size_t)(size), __FILE__, __FUNCTION__, __LINE__)
    #define OS_FREE(ptr) \
        do {\
            if (ptr) {\
                memory_detect_free((void *)(ptr), __FILE__, __FUNCTION__, __LINE__);\
                (ptr) = NULL;\
            }\
        } while (0)
    #define OS_STRDUP(str) memory_detect_strdup((void *)(str), __FILE__, __FUNCTION__, __LINE__)
    #define OS_STREQUAL(str1, str2) (((str1) && (str2)) ? (strcmp((str1), (str2)) == 0) : false)
    #define OS_MEMORY_DUMP() memory_detect_dump()

#endif

#ifdef __cplusplus
}
#endif

#endif /* __MEMORY_LEAK_DETECT_H__ */
