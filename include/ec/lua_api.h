/* Lua entry points of libeditingcore. The host links its own Lua. */
#ifndef EC_LUA_API_H
#define EC_LUA_API_H

#include <lua.h>

#ifdef __cplusplus
extern "C" {
#endif

int ec_luaopen_process(lua_State *L);     /* returns the "process" module */
int ec_luaopen_regex(lua_State *L);       /* returns the "regex" module */
int ec_luaopen_dirmonitor(lua_State *L);  /* returns the "dirmonitor" metatable */

/* Adds the shared functions to the table on top of the stack: rmdir, chdir,
** mkdir, list_dir, absolute_path, get_file_info, get_fs_type, ftruncate,
** get_process_id, get_time, sleep, exec, fuzzy_match, path_compare, setenv. */
void ec_system_register(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif
