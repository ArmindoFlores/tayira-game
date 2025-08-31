#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "watchdog.h"
#include "data_structures/hashtable.h"
#include "data_structures/linked_list.h"
#include "logger/logger.h"
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <sys/stat.h>

#if defined(__APPLE__)
    #define STAT_MTIME_SEC(st)  ((st).st_mtimespec.tv_sec)
    #define STAT_MTIME_NSEC(st) ((st).st_mtimespec.tv_nsec)
#elif defined(__linux__)
    #define STAT_MTIME_SEC(st)  ((st).st_mtim.tv_sec)
    #define STAT_MTIME_NSEC(st) ((st).st_mtim.tv_nsec)
#else
    #define STAT_MTIME_SEC(st)  ((st).st_mtime)
    #define STAT_MTIME_NSEC(st) (0L)
#endif

struct watchdog_handler_s {
    hashtable watched_files;
    watchdog_callback cb;
    void *cb_args;
    mtx_t files_mtx;    
};

typedef struct watchdog_s {
    linked_list handlers;
    mtx_t handlers_mtx;    
    thrd_t thread;
    int stop;
} *watchdog;

static watchdog watchdog_singleton = NULL;

void watchdog_init() {
    if (watchdog_singleton != NULL) return;
    watchdog_singleton = (watchdog) malloc(sizeof(struct watchdog_s));
    if (watchdog_singleton == NULL) {
        return;
    }
    watchdog_singleton->stop = 0;
    watchdog_singleton->thread = 0;
    if (mtx_init(&watchdog_singleton->handlers_mtx, mtx_plain) != thrd_success) {
        free(watchdog_singleton);
        watchdog_singleton = NULL;
        return;
    }
    watchdog_singleton->handlers = linked_list_create();
    if (watchdog_singleton->handlers == NULL) {
        watchdog_cleanup();
        return;
    }
}

watchdog_handler watchdog_get_handler(watchdog_callback cb, void *cb_args) {
    if (watchdog_singleton == NULL) return NULL;
    watchdog_handler handler = (watchdog_handler) calloc(1, sizeof(struct watchdog_handler_s));
    if (handler == NULL) {
        return NULL;
    }
    if (mtx_init(&handler->files_mtx, mtx_plain) != thrd_success) {
        free(handler);
        return NULL;
    }
    handler->watched_files = hashtable_create();
    if (handler->watched_files == NULL) {
        watchdog_destroy_handler(handler);
        return NULL;
    }
    handler->cb = cb;
    handler->cb_args = cb_args;
    mtx_lock(&watchdog_singleton->handlers_mtx);
    if (linked_list_pushfront(watchdog_singleton->handlers, handler) != 0) {
        mtx_unlock(&watchdog_singleton->handlers_mtx);
        watchdog_destroy_handler(handler);
        return NULL;
    }
    mtx_unlock(&watchdog_singleton->handlers_mtx);
    return handler;
}

static int remove_if_equals(void *element, void *args) {
    return element == args;
}

static iteration_result destroy_timespec(const hashtable_entry *entry) {
    free(entry->value);
    return ITERATION_CONTINUE;
}

void watchdog_destroy_handler(watchdog_handler handler) {
    if (handler == NULL) return;
    mtx_lock(&watchdog_singleton->handlers_mtx);
    linked_list_remove_if(watchdog_singleton->handlers, remove_if_equals, handler);
    mtx_unlock(&watchdog_singleton->handlers_mtx);
    mtx_destroy(&handler->files_mtx);
    if (handler->watched_files != NULL) {
        hashtable_foreach(handler->watched_files, destroy_timespec);
        hashtable_destroy(handler->watched_files);
    }
    free(handler);
}

int watchdog_watch(watchdog_handler handler, const char *file) {
    struct timespec *value = malloc(sizeof(struct timespec));
    value->tv_sec = 0;
    value->tv_nsec = 0;
    mtx_lock(&handler->files_mtx);
    int result = hashtable_set(handler->watched_files, file, value);
    mtx_unlock(&handler->files_mtx);
    return result;
}

void watchdog_forget(watchdog_handler handler, const char *file) {
    mtx_lock(&handler->files_mtx);
    hashtable_delete(handler->watched_files, file);
    mtx_unlock(&handler->files_mtx);
}

static time_t compare_timespecs(struct timespec *t1, struct timespec *t2) {
    if (t1->tv_sec == t2->tv_sec) {
        return t1->tv_nsec - t2->tv_nsec;
    }
    return t1->tv_sec - t2->tv_sec;
}

struct watch_single_file_args_s {
    watchdog_handler handler;
    linked_list to_remove;
};

static iteration_result watch_single_file(const hashtable_entry *entry, void *_args) {
    struct watch_single_file_args_s *args = (struct watch_single_file_args_s *) _args;
    const char *file = entry->key;
    struct timespec *last_modified = (struct timespec *) entry->value;
    if (last_modified == NULL) return ITERATION_CONTINUE;

    struct stat file_stat;
    struct timespec file_ts;
    if (stat(file, &file_stat) == 0) {
        file_ts.tv_sec = STAT_MTIME_SEC(file_stat);
        file_ts.tv_nsec = STAT_MTIME_NSEC(file_stat);

        if (compare_timespecs(last_modified, &file_ts) < 0) {
            if (last_modified->tv_sec != 0) {
                args->handler->cb(file, WATCHDOG_FILE_CHANGED, args->handler->cb_args);
            }
            *last_modified = file_ts;
        }
    } else {
        free(last_modified);
        linked_list_pushfront(args->to_remove, file);
    }

    return ITERATION_CONTINUE;
}

static iteration_result watch_handler(void *element) {
    watchdog_handler handler = (watchdog_handler) element;

    struct watch_single_file_args_s watch_single_file_args = {
        .handler = handler,
        .to_remove = linked_list_create()
    };

    if (watch_single_file_args.to_remove == NULL) {
        log_error("Watchdog thread error: failed to allocate memory");
        return ITERATION_CONTINUE;
    }

    mtx_lock(&handler->files_mtx);
    hashtable_foreach_args(handler->watched_files, watch_single_file, &watch_single_file_args);
    while (1) {
        char *file_to_remove = (char *) linked_list_popfront(watch_single_file_args.to_remove);
        if (file_to_remove == NULL) break;
        free(hashtable_delete(handler->watched_files, file_to_remove));
    }
    mtx_unlock(&handler->files_mtx);

    linked_list_destroy(watch_single_file_args.to_remove);

    return ITERATION_CONTINUE;
}

int watchdog_thread_function(void *_) {
    (void) _;
    while (!watchdog_singleton->stop) {
        mtx_lock(&watchdog_singleton->handlers_mtx);
        linked_list_foreach(watchdog_singleton->handlers, watch_handler);
        mtx_unlock(&watchdog_singleton->handlers_mtx);

        // Sleep for 500ms
        thrd_sleep(&(struct timespec){ .tv_sec = 0, .tv_nsec = 500000000 }, NULL);
    }
    
    return 0;
}

int watchdog_run() {
    if (watchdog_singleton == NULL) return 1;
    if (watchdog_singleton->thread != 0) {
        log_error("Watchdog thread is already running");
        return 1;
    }
    if (thrd_create(&watchdog_singleton->thread, watchdog_thread_function, NULL) != thrd_success) {
        log_error("Failed to create watchdog thread");
        return 1;
    }
    return 0;
}

void watchdog_stop() {
    if (watchdog_singleton->stop == 1 || watchdog_singleton->thread == 0) return;
    watchdog_singleton->stop = 1;
    thrd_join(watchdog_singleton->thread, NULL);
}

static iteration_result destroy_handler(void *element) {
    watchdog_handler handler = (watchdog_handler) element;
    watchdog_destroy_handler(handler);
    return ITERATION_CONTINUE;
}

void watchdog_cleanup() {
    if (watchdog_singleton == NULL) return;
    watchdog_stop();
    if (watchdog_singleton->handlers != NULL) {
        linked_list_foreach(watchdog_singleton->handlers, destroy_handler);
        linked_list_destroy(watchdog_singleton->handlers);
    }
    mtx_destroy(&watchdog_singleton->handlers_mtx);
    free(watchdog_singleton);
    watchdog_singleton = NULL;
}
