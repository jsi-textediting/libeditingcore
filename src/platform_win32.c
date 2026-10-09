/* Win32 implementation of ec/platform.h. */
#ifdef _WIN32
#include <ec/platform.h>
#include <ec/utfconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#ifdef _MSC_VER
static __declspec(thread) char error_buf[512];
#else
static _Thread_local char error_buf[512];
#endif

/* Sets "<what>: <system message>" (or just <what> when code is 0), like
 * SDL's WIN_SetErrorFromHRESULT did. */
static void set_error(const char *what, DWORD code) {
  WCHAR *wmsg = NULL;
  char *msg = NULL;
  if (code) {
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM
                  | FORMAT_MESSAGE_ALLOCATE_BUFFER
                  | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPWSTR) &wmsg, 0, NULL);
    if (wmsg) {
      msg = utfconv_wctoutf8(wmsg);
      LocalFree(wmsg);
    }
  }
  if (msg) {
    /* strip the trailing "\r\n" (and period) FormatMessage appends */
    size_t len = strlen(msg);
    while (len && (msg[len - 1] == '\r' || msg[len - 1] == '\n' || msg[len - 1] == '.'))
      msg[--len] = '\0';
    snprintf(error_buf, sizeof(error_buf), "%s: %s", what, msg);
    free(msg);
  } else if (code) {
    snprintf(error_buf, sizeof(error_buf), "%s (error %lu)", what, (unsigned long) code);
  } else {
    snprintf(error_buf, sizeof(error_buf), "%s", what);
  }
}

const char *ec_get_error(void) {
  return error_buf;
}

/* ── time ─────────────────────────────────────────────────────────────── */

uint64_t ec_ticks_ms(void) {
  static uint64_t start;
  uint64_t now = GetTickCount64();
  if (!start) start = now;
  return now - start;
}

double ec_time_seconds(void) {
  LARGE_INTEGER freq, count;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&count);
  return (double) count.QuadPart / (double) freq.QuadPart;
}

void ec_sleep_ms(uint32_t ms) {
  Sleep(ms);
}

/* ── threads ──────────────────────────────────────────────────────────── */

struct ec_mutex { CRITICAL_SECTION cs; };
struct ec_cond { CONDITION_VARIABLE cv; };
struct ec_thread { HANDLE handle; ec_thread_fn fn; void *data; int status; };

ec_mutex *ec_mutex_create(void) {
  ec_mutex *mutex = malloc(sizeof(*mutex));
  if (mutex) InitializeCriticalSection(&mutex->cs);
  else set_error("cannot create mutex: out of memory", 0);
  return mutex;
}

void ec_mutex_lock(ec_mutex *mutex) { if (mutex) EnterCriticalSection(&mutex->cs); }
void ec_mutex_unlock(ec_mutex *mutex) { if (mutex) LeaveCriticalSection(&mutex->cs); }

void ec_mutex_destroy(ec_mutex *mutex) {
  if (!mutex) return;
  DeleteCriticalSection(&mutex->cs);
  free(mutex);
}

ec_cond *ec_cond_create(void) {
  ec_cond *cond = malloc(sizeof(*cond));
  if (cond) InitializeConditionVariable(&cond->cv);
  else set_error("cannot create condition variable: out of memory", 0);
  return cond;
}

void ec_cond_wait(ec_cond *cond, ec_mutex *mutex) {
  if (cond && mutex) SleepConditionVariableCS(&cond->cv, &mutex->cs, INFINITE);
}

void ec_cond_signal(ec_cond *cond) { if (cond) WakeConditionVariable(&cond->cv); }

void ec_cond_destroy(ec_cond *cond) { free(cond); }

static DWORD WINAPI thread_main(LPVOID arg) {
  ec_thread *thread = arg;
  thread->status = thread->fn(thread->data);
  return 0;
}

ec_thread *ec_thread_create(ec_thread_fn fn, const char *name, void *data) {
  (void) name;
  ec_thread *thread = calloc(1, sizeof(*thread));
  if (!thread) { set_error("cannot create thread: out of memory", 0); return NULL; }
  thread->fn = fn;
  thread->data = data;
  thread->handle = CreateThread(NULL, 0, thread_main, thread, 0, NULL);
  if (!thread->handle) {
    set_error("cannot create thread", GetLastError());
    free(thread);
    return NULL;
  }
  return thread;
}

void ec_thread_join(ec_thread *thread, int *status) {
  if (!thread) return;
  WaitForSingleObject(thread->handle, INFINITE);
  CloseHandle(thread->handle);
  if (status) *status = thread->status;
  free(thread);
}

/* ── files and environment ────────────────────────────────────────────── */

bool ec_enumerate_directory(const char *path, ec_enum_callback callback, void *userdata) {
  size_t len = strlen(path);
  char *pattern = malloc(len + 3);
  if (!pattern) { set_error("cannot open directory: out of memory", 0); return false; }
  memcpy(pattern, path, len);
  if (len && path[len - 1] != '/' && path[len - 1] != '\\') pattern[len++] = '\\';
  pattern[len++] = '*';
  pattern[len] = '\0';
  LPWSTR wpattern = utfconv_utf8towc(pattern);
  free(pattern);
  if (!wpattern) { set_error("invalid path", GetLastError()); return false; }

  WIN32_FIND_DATAW data;
  HANDLE handle = FindFirstFileW(wpattern, &data);
  free(wpattern);
  if (handle == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    if (err == ERROR_FILE_NOT_FOUND) return true; /* empty */
    set_error("cannot open directory", err);
    return false;
  }
  ec_enum_result result = EC_ENUM_CONTINUE;
  do {
    if (!wcscmp(data.cFileName, L".") || !wcscmp(data.cFileName, L"..")) continue;
    char *name = utfconv_wctoutf8(data.cFileName);
    if (!name) continue;
    result = callback(userdata, path, name);
    free(name);
  } while (result == EC_ENUM_CONTINUE && FindNextFileW(handle, &data));
  FindClose(handle);
  return true;
}

bool ec_remove_path(const char *path) {
  LPWSTR wpath = utfconv_utf8towc(path);
  if (!wpath) { set_error("invalid path", GetLastError()); return false; }
  DWORD attrs = GetFileAttributesW(wpath);
  bool ok;
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    DWORD err = GetLastError();
    ok = err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND;
    if (!ok) set_error("cannot remove path", err);
  } else {
    ok = (attrs & FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectoryW(wpath) : DeleteFileW(wpath);
    if (!ok) set_error("cannot remove path", GetLastError());
  }
  free(wpath);
  return ok;
}

int ec_setenv(const char *name, const char *value, int overwrite) {
  if (!overwrite && getenv(name)) return 0;
  return _putenv_s(name, value);
}

#endif /* _WIN32 */
