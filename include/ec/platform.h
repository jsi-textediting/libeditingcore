/* Small portability layer used by libeditingcore instead of SDL: threads,
** mutexes, condition variables, timers and a few file system helpers.
** POSIX (pthreads/libc) and Win32 implementations live in src/platform_*.c. */
#ifndef EC_PLATFORM_H
#define EC_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* memory: plain libc */
#define ec_zero(x) memset(&(x), 0, sizeof(x))

/* last error set by the functions below that can fail (thread local) */
const char *ec_get_error(void);

/* time */
uint64_t ec_ticks_ms(void);              /* monotonic, since first call */
double ec_time_seconds(void);            /* monotonic, high resolution */
void ec_sleep_ms(uint32_t ms);

/* threads */
typedef struct ec_mutex ec_mutex;
typedef struct ec_cond ec_cond;
typedef struct ec_thread ec_thread;
typedef int (*ec_thread_fn)(void *data);

ec_mutex *ec_mutex_create(void);
void ec_mutex_lock(ec_mutex *mutex);
void ec_mutex_unlock(ec_mutex *mutex);
void ec_mutex_destroy(ec_mutex *mutex);
ec_cond *ec_cond_create(void);
void ec_cond_wait(ec_cond *cond, ec_mutex *mutex);
void ec_cond_signal(ec_cond *cond);
void ec_cond_destroy(ec_cond *cond);
ec_thread *ec_thread_create(ec_thread_fn fn, const char *name, void *data);
void ec_thread_join(ec_thread *thread, int *status);

/* files and environment (paths are UTF-8) */
typedef enum { EC_ENUM_CONTINUE, EC_ENUM_STOP } ec_enum_result;
typedef ec_enum_result (*ec_enum_callback)(void *userdata, const char *dirname, const char *fname);

/* skips "." and "..", returns false (and sets the error) if the directory can't be opened */
bool ec_enumerate_directory(const char *path, ec_enum_callback callback, void *userdata);
/* removes a file or an empty directory; a missing path is success */
bool ec_remove_path(const char *path);
int ec_setenv(const char *name, const char *value, int overwrite);

#ifdef __cplusplus
}
#endif

#endif
