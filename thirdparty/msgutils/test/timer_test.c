#include <stdio.h>
#include "msgutils/os_timer.h"
#include "msgutils/os_thread.h"
#include "msgutils/os_logger.h"

#define LOG_TAG "timertest"

static void oneshot_timer_cb()
{
    static int i = 0;
    OS_LOGD(LOG_TAG, "-->count=[%d]", i++);
}

static void reload_timer_cb()
{
    static int i = 0;
    OS_LOGD(LOG_TAG, "-->count=[%d]", i++);
    OS_THREAD_SLEEP_MSEC(400);
}

int main()
{
    struct os_timerattr oneshot_attr = {
        .name = "oneshot_timer",
        .period_ms = 500,
        .reload = false,
    };

    struct os_timerattr reload_attr = {
        .name = "reload_timer",
        .period_ms = 500,
        .reload = true,
    };

    os_timer_t oneshot_timer, reload_timer;

    OS_LOGD(LOG_TAG, "create oneshot_timer");
    oneshot_timer = OS_TIMER_CREATE(&oneshot_attr, oneshot_timer_cb);

    OS_LOGD(LOG_TAG, "create reload_timer");
    reload_timer = OS_TIMER_CREATE(&reload_attr, reload_timer_cb);

    OS_LOGD(LOG_TAG, "start oneshot_timer");
    OS_TIMER_START(oneshot_timer);

    OS_LOGD(LOG_TAG, "start reload_timer");
    OS_TIMER_START(reload_timer);

    OS_LOGD(LOG_TAG, "oneshot_timer state: [%s]", OS_TIMER_IS_ACTIVE(oneshot_timer) ? "active" : "inactive");
    OS_LOGD(LOG_TAG, "reload_timer state: [%s]", OS_TIMER_IS_ACTIVE(reload_timer) ? "active" : "inactive");

    OS_LOGD(LOG_TAG, "sleep 5000 ms");
    OS_THREAD_SLEEP_MSEC(5000);

    OS_LOGD(LOG_TAG, "oneshot_timer state: [%s]", OS_TIMER_IS_ACTIVE(oneshot_timer) ? "active" : "inactive");
    OS_LOGD(LOG_TAG, "reload_timer state: [%s]", OS_TIMER_IS_ACTIVE(reload_timer) ? "active" : "inactive");

    OS_LOGD(LOG_TAG, "stop oneshot_timer");
    OS_TIMER_STOP(oneshot_timer);

    OS_LOGD(LOG_TAG, "stop reload_timer");
    OS_TIMER_STOP(reload_timer);

    OS_LOGD(LOG_TAG, "oneshot_timer state: [%s]", OS_TIMER_IS_ACTIVE(oneshot_timer) ? "active" : "inactive");
    OS_LOGD(LOG_TAG, "reload_timer state: [%s]", OS_TIMER_IS_ACTIVE(reload_timer) ? "active" : "inactive");

    OS_LOGD(LOG_TAG, "sleep 2000 ms");
    OS_THREAD_SLEEP_MSEC(2000);

    OS_LOGD(LOG_TAG, "start oneshot_timer again");
    OS_TIMER_START(oneshot_timer);

    OS_LOGD(LOG_TAG, "start reload_timer again");
    OS_TIMER_START(reload_timer);

    OS_LOGD(LOG_TAG, "sleep 3000 ms");
    OS_THREAD_SLEEP_MSEC(3000);

    OS_LOGD(LOG_TAG, "destroy oneshot_timer");
    OS_TIMER_DESTROY(oneshot_timer);

    OS_LOGD(LOG_TAG, "destroy reload_timer");
    OS_TIMER_DESTROY(reload_timer);

    return 0;
}
