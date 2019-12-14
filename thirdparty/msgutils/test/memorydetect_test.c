#include <stdio.h>
#include <unistd.h>

#include "msgutils/os_memory.h"

int main()
{
    void *ptr[6];

    //OS_MEMORY_DUMP();

    ptr[0] = OS_MALLOC(1);
    ptr[1] = OS_MALLOC(2);
    ptr[2] = OS_MALLOC(3);
    ptr[3] = OS_MALLOC(4);
    ptr[4] = OS_MALLOC(5);
    OS_MEMORY_DUMP();

    OS_FREE(ptr[3]);
    OS_MEMORY_DUMP();

    ptr[5] = OS_REALLOC(ptr[2], 6);
    printf("%p, %p\n", ptr[2], ptr[5]);
    OS_MEMORY_DUMP();

    // produce overflow
    sprintf(ptr[4], "hello, world");
    OS_MEMORY_DUMP();

    return 0;
}


