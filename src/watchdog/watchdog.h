#ifndef _H_WATCHDOG_H_
#define _H_WATCHDOG_H_

typedef struct watchdog_handler_s *watchdog_handler;
typedef enum watchdog_event {
    WATCHDOG_FILE_CHANGED,
    WATCHDOG_FILE_DELETED
} watchdog_event;
typedef void (*watchdog_callback) (const char *file, watchdog_event, void *args);

void watchdog_init();
watchdog_handler watchdog_get_handler(watchdog_callback, void *args);
void watchdog_destroy_handler(watchdog_handler);
int watchdog_watch(watchdog_handler, const char *file);
void watchdog_forget(watchdog_handler, const char *file);
int watchdog_run();
void watchdog_stop();
void watchdog_cleanup();

#endif
