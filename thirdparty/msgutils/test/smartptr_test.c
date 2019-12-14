#include <stdio.h>
#include "msgutils/os_memory.h"
#include "msgutils/os_logger.h"
#include "msgutils/os_thread.h"
#include "msgutils/smartptr.h"

#define LOG_TAG "smartptr_test"

struct smartptr_test {
    int val;
    const char *str;
};

static void smartptr_test_free(void *data)
{
    struct smartptr_test *ptr = (struct smartptr_test *)data;

    OS_LOGD(LOG_TAG, "Free %p, str=%s", ptr, ptr->str);
    OS_FREE(ptr->str);
}

int main()
{
    struct smartptr_test *ptr = NULL;

    ptr = SMARTPTR_NEW(sizeof(struct smartptr_test), smartptr_test_free);
    OS_LOGD(LOG_TAG, "New %p", ptr);
    SMARTPTR_DUMP();

    ptr->str = OS_STRDUP("hello");

    OS_LOGD(LOG_TAG, "Get %p", ptr);
    SMARTPTR_GET(ptr);
    SMARTPTR_DUMP();

    OS_LOGD(LOG_TAG, "Put %p", ptr);
    SMARTPTR_PUT(ptr);
    SMARTPTR_DUMP();

    OS_LOGD(LOG_TAG, "Put %p", ptr);
    SMARTPTR_PUT(ptr);
    SMARTPTR_DUMP();

    return 0;
}
