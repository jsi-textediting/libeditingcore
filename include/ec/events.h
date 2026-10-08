/* Host hooks for the asynchronous events the library produces (currently only
** "dirmonitor": a watched directory changed). The library never depends on
** how the host delivers them: an SDL custom event in an editor, a self-pipe in
** a server. Without hooks, registering succeeds and pushing does nothing. */
#ifndef EC_EVENTS_H
#define EC_EVENTS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*ec_event_register_fn)(const char *name, void *userdata);
/* may be called from a library thread */
typedef bool (*ec_event_push_fn)(const char *name, void *userdata);

void ec_set_event_hooks(ec_event_register_fn register_fn, ec_event_push_fn push_fn, void *userdata);

/* used inside the library */
bool ec_event_register(const char *name);
bool ec_event_push(const char *name);

#ifdef __cplusplus
}
#endif

#endif
