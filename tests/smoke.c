/* Runs smoke.lua with the library's modules registered. */
#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <ec/events.h>
#include <ec/lua_api.h>

static int events_seen;

static bool on_register(const char *name, void *ud) { (void) name; (void) ud; return true; }
static bool on_push(const char *name, void *ud) { (void) name; (void) ud; events_seen++; return true; }

static int l_events_seen(lua_State *L) { lua_pushinteger(L, events_seen); return 1; }

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: ec_smoke script.lua\n"); return 2; }
  ec_set_event_hooks(on_register, on_push, NULL);
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  luaL_requiref(L, "process", ec_luaopen_process, 1);
  luaL_requiref(L, "regex", ec_luaopen_regex, 1);
  luaL_requiref(L, "dirmonitor", ec_luaopen_dirmonitor, 1);
  lua_newtable(L);
  ec_system_register(L);
  lua_setglobal(L, "system");
  lua_pushcfunction(L, l_events_seen);
  lua_setglobal(L, "events_seen");
  int rc = 0;
  if (luaL_dofile(L, argv[1]) != LUA_OK) {
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    rc = 1;
  }
  lua_close(L);
  return rc;
}
