#include "watchdog.h"
#include "data_structures/hashtable.h"
#include "data_structures/linked_list.h"
#include "logger/logger.h"
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <sys/stat.h>

struct watchdog_s {
    hashtable watched_files;
    mtx_t files_mtx;

    watchdog_callback cb;
    void *cb_args;
    
    thrd_t thread;
    int stop;
};

watchdog watchdog_create(watchdog_callback cb, void *cb_args) {
    watchdog w = (watchdog) malloc(sizeof(struct watchdog_s));
    if (w == NULL) {
        return NULL;
    }
    w->stop = 0;
    w->thread = 0;
    w->cb = cb;
    w->cb_args = cb_args;
    if (mtx_init(&w->files_mtx, mtx_plain) != thrd_success) {
        free(w);
        return NULL;
    }
    w->watched_files = hashtable_create();
    if (w->watched_files == NULL) {
        watchdog_destroy(w);
        return NULL;
    }
    return w;
}

int watchdog_watch(watchdog w, const char *file) {
    struct timespec *value = malloc(sizeof(struct timespec));
    value->tv_sec = 0;
    value->tv_nsec = 0;
    mtx_lock(&w->files_mtx);
    int result = hashtable_set(w->watched_files, file, value);
    mtx_unlock(&w->files_mtx);
    return result;
}

void watchdog_forget(watchdog w, const char *file) {
    mtx_lock(&w->files_mtx);
    hashtable_delete(w->watched_files, file);
    mtx_unlock(&w->files_mtx);
}

static time_t compare_timespecs(struct timespec *t1, struct timespec *t2) {
    if (t1->tv_sec == t2->tv_sec) {
        return t1->tv_nsec - t2->tv_nsec;
    }
    return t1->tv_sec - t2->tv_sec;
}

struct watch_single_file_args_s {
    watchdog w;
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
        file_ts.tv_sec = file_stat.st_mtime;
        file_ts.tv_nsec = file_stat.st_mtimensec;

        if (compare_timespecs(last_modified, &file_ts) < 0) {
            if (last_modified->tv_sec != 0) {
                args->w->cb(file, WATCHDOG_FILE_CHANGED, args->w->cb_args);
            }
            *last_modified = file_ts;
        }
    } else {
        free(last_modified);
        linked_list_pushfront(args->to_remove, file);
    }

    return ITERATION_CONTINUE;
}

int watchdog_thread_function(void *_args) {
    watchdog w = (watchdog) _args;

    struct watch_single_file_args_s watch_single_file_args = {
        .w = w,
        .to_remove = linked_list_create()
    };

    if (watch_single_file_args.to_remove == NULL) {
        log_error("Watchdog thread exited: failed to allocate memory");
        return 1;
    }

    while (!w->stop) {
        mtx_lock(&w->files_mtx);
        hashtable_foreach_args(w->watched_files, watch_single_file, &watch_single_file_args);
        while (1) {
            char *file_to_remove = (char *) linked_list_popfront(watch_single_file_args.to_remove);
            if (file_to_remove == NULL) break;
            free(hashtable_delete(w->watched_files, file_to_remove));
        }
        mtx_unlock(&w->files_mtx);

        // Sleep for 500ms
        thrd_sleep(&(struct timespec){ .tv_sec = 0, .tv_nsec = 500000000 }, NULL);
    }

    linked_list_destroy(watch_single_file_args.to_remove);
    return 0;
}

int watchdog_run(watchdog w) {
    if (w->thread != 0) {
        log_error("Watchdog thread is already running");
        return 1;
    }
    if (thrd_create(&w->thread, watchdog_thread_function, w) != thrd_success) {
        log_error("Failed to create watchdog thread");
        return 1;
    }
    return 0;
}

void watchdog_stop(watchdog w) {
    if (w->stop == 1 || w->thread == 0) return;
    w->stop = 1;
    thrd_join(w->thread, NULL);
}

static iteration_result destroy_timespec(const hashtable_entry *entry) {
    free(entry->value);
    return ITERATION_CONTINUE;
}

void watchdog_destroy(watchdog w) {
    if (w == NULL) return;
    watchdog_stop(w);
    if (w->watched_files != NULL) {
        hashtable_foreach(w->watched_files, destroy_timespec);
        hashtable_destroy(w->watched_files);
    }
    mtx_destroy(&w->files_mtx);
    free(w);
}
