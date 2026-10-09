/* POSIX implementation of ec/platform.h. */
#define _GNU_SOURCE
#include <ec/platform.h>

#ifndef _WIN32
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static _Thread_local char error_buf[512];

static void set_error(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(error_buf, sizeof(error_buf), fmt, ap);
  va_end(ap);
}

const char *ec_get_error(void) {
  return error_buf;
}

/* ── time ─────────────────────────────────────────────────────────────── */

static uint64_t monotonic_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t) ts.tv_sec * 1000000000u + (uint64_t) ts.tv_nsec;
}

static uint64_t start_ns;
static pthread_once_t start_once = PTHREAD_ONCE_INIT;
static void init_start(void) { start_ns = monotonic_ns(); }

uint64_t ec_ticks_ms(void) {
  pthread_once(&start_once, init_start);
  return (monotonic_ns() - start_ns) / 1000000u;
}

double ec_time_seconds(void) {
  return monotonic_ns() / 1e9;
}

void ec_sleep_ms(uint32_t ms) {
  struct timespec ts = { (time_t) (ms / 1000), (long) (ms % 1000) * 1000000L };
  while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
}

/* ── threads ──────────────────────────────────────────────────────────── */

struct ec_mutex { pthread_mutex_t m; };
struct ec_cond { pthread_cond_t c; };
struct ec_thread { pthread_t t; ec_thread_fn fn; void *data; int status; };

/* Recursive, like the SDL mutexes this replaces and the Win32 critical
 * section: f_dirmonitor_check runs a Lua callback with the lock held. */
ec_mutex *ec_mutex_create(void) {
  ec_mutex *mutex = malloc(sizeof(*mutex));
  if (mutex) {
    pthread_mutexattr_t attr;
    int err = pthread_mutexattr_init(&attr);
    if (!err) {
      err = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      if (!err) err = pthread_mutex_init(&mutex->m, &attr);
      pthread_mutexattr_destroy(&attr);
    }
    if (err) { free(mutex); mutex = NULL; }
  }
  if (!mutex) set_error("cannot create mutex");
  return mutex;
}

void ec_mutex_lock(ec_mutex *mutex) { if (mutex) pthread_mutex_lock(&mutex->m); }
void ec_mutex_unlock(ec_mutex *mutex) { if (mutex) pthread_mutex_unlock(&mutex->m); }

void ec_mutex_destroy(ec_mutex *mutex) {
  if (!mutex) return;
  pthread_mutex_destroy(&mutex->m);
  free(mutex);
}

ec_cond *ec_cond_create(void) {
  ec_cond *cond = malloc(sizeof(*cond));
  if (cond && pthread_cond_init(&cond->c, NULL) != 0) { free(cond); cond = NULL; }
  if (!cond) set_error("cannot create condition variable");
  return cond;
}

/* The mutex is recursive, but pthread_cond_wait only releases one level of
 * it: callers must hold it exactly once when waiting (SDL had the same rule). */
void ec_cond_wait(ec_cond *cond, ec_mutex *mutex) {
  if (cond && mutex) pthread_cond_wait(&cond->c, &mutex->m);
}

void ec_cond_signal(ec_cond *cond) { if (cond) pthread_cond_signal(&cond->c); }

void ec_cond_destroy(ec_cond *cond) {
  if (!cond) return;
  pthread_cond_destroy(&cond->c);
  free(cond);
}

static void *thread_main(void *arg) {
  ec_thread *thread = arg;
  thread->status = thread->fn(thread->data);
  return NULL;
}

ec_thread *ec_thread_create(ec_thread_fn fn, const char *name, void *data) {
  (void) name;
  ec_thread *thread = calloc(1, sizeof(*thread));
  if (!thread) { set_error("out of memory"); return NULL; }
  thread->fn = fn;
  thread->data = data;
  int rc = pthread_create(&thread->t, NULL, thread_main, thread);
  if (rc != 0) {
    set_error("cannot create thread: %s", strerror(rc));
    free(thread);
    return NULL;
  }
  return thread;
}

void ec_thread_join(ec_thread *thread, int *status) {
  if (!thread) return;
  pthread_join(thread->t, NULL);
  if (status) *status = thread->status;
  free(thread);
}

/* ── files and environment ────────────────────────────────────────────── */

bool ec_enumerate_directory(const char *path, ec_enum_callback callback, void *userdata) {
  DIR *dir = opendir(path);
  if (!dir) {
    set_error("cannot open directory '%s': %s", path, strerror(errno));
    return false;
  }
  ec_enum_result result = EC_ENUM_CONTINUE;
  struct dirent *entry;
  while (result == EC_ENUM_CONTINUE && (entry = readdir(dir)) != NULL) {
    const char *name = entry->d_name;
    if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
    result = callback(userdata, path, name);
  }
  closedir(dir);
  return true;
}

bool ec_remove_path(const char *path) {
  if (remove(path) == 0 || errno == ENOENT) return true;
  set_error("cannot remove '%s': %s", path, strerror(errno));
  return false;
}

int ec_setenv(const char *name, const char *value, int overwrite) {
  return setenv(name, value, overwrite);
}

#endif /* !_WIN32 */
