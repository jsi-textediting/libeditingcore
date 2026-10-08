#include <ec/events.h>

static ec_event_register_fn register_hook;
static ec_event_push_fn push_hook;
static void *hook_data;

void ec_set_event_hooks(ec_event_register_fn register_fn, ec_event_push_fn push_fn, void *userdata) {
  register_hook = register_fn;
  push_hook = push_fn;
  hook_data = userdata;
}

bool ec_event_register(const char *name) {
  return register_hook ? register_hook(name, hook_data) : true;
}

bool ec_event_push(const char *name) {
  return push_hook ? push_hook(name, hook_data) : true;
}
