#ifndef _H_WATCHDOG_H_
#define _H_WATCHDOG_H_

typedef struct watchdog_s *watchdog;
typedef enum watchdog_event {
    WATCHDOG_FILE_CHANGED,
    WATCHDOG_FILE_DELETED
} watchdog_event;
typedef void (*watchdog_callback) (const char *file, watchdog_event, void *args);

watchdog watchdog_create(watchdog_callback cb, void *cb_args);
int watchdog_watch(watchdog, const char *file);
void watchdog_forget(watchdog, const char *file);
int watchdog_run(watchdog);
void watchdog_stop(watchdog);
void watchdog_destroy(watchdog);

#endif
