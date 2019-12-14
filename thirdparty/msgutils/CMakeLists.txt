cmake_minimum_required(VERSION 2.8)

# include files
include_directories(${CMAKE_SOURCE_DIR}/include)

# source files
set (LIBS_SRC ${CMAKE_SOURCE_DIR}/source/memory_detect.c
              ${CMAKE_SOURCE_DIR}/source/msglooper.c
              ${CMAKE_SOURCE_DIR}/source/msgqueue.c
              ${CMAKE_SOURCE_DIR}/source/ringbuf.c
              ${CMAKE_SOURCE_DIR}/source/smartptr.c
              ${CMAKE_SOURCE_DIR}/source/stllist.c
              ${CMAKE_SOURCE_DIR}/source/sw_timer.c
              ${CMAKE_SOURCE_DIR}/source/sw_watchdog.c
              ${CMAKE_SOURCE_DIR}/source/portable/os_logger.c
              ${CMAKE_SOURCE_DIR}/source/portable/os_thread.c
              ${CMAKE_SOURCE_DIR}/source/portable/os_time.c
              ${CMAKE_SOURCE_DIR}/source/portable/os_timer.c
)

# cflags: OS_LINUX, OS_ANDROID, OS_MACOSX, OS_IOS, OS_FREERTOS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Werror -DOS_MACOSX")

# ldflags
set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")

# ENABLE_MEMORY_LEAK_DETECT
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_MEMORY_LEAK_DETECT")

# ENABLE_MEMORY_OVERFLOW_DETECT
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_MEMORY_OVERFLOW_DETECT")

# ENABLE_SMARTPTR_DETECT
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_SMARTPTR_DETECT")

message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")

# msgutils lib
add_library(msgutils   SHARED ${LIBS_SRC})
add_library(msgutils_s STATIC ${LIBS_SRC})
