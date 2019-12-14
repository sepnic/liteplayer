#include <stdio.h>
#include "msgutils/os_time.h"
#include "msgutils/os_logger.h"

#define LOG_TAG "timetest"

int main()
{
    struct os_realtime now;
    unsigned long seconds;

    seconds = OS_TIMESTAMP_TO_UTC(&now);
    OS_LOGD(LOG_TAG, "OS_TIMESTAMP_TO_UTC: %d, [%04d%02d%02d-%02d%02d%02d:%03d]",
           seconds, now.year, now.mon, now.day, now.hour, now.min, now.sec, now.msec);


    seconds = OS_TIMESTAMP_TO_LOCAL(&now);
    OS_LOGD(LOG_TAG, "OS_TIMESTAMP_TO_LOCAL: %d, [%04d%02d%02d-%02d%02d%02d:%03d]",
           seconds, now.year, now.mon, now.day, now.hour, now.min, now.sec, now.msec);

    OS_LOGD(LOG_TAG, "OS_MONOTONIC_USEC: %llu", OS_MONOTONIC_USEC());
    OS_LOGD(LOG_TAG, "OS_REALTIME_USEC: %llu", OS_REALTIME_USEC());

    return 0;
}
