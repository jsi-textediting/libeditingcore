#ifndef EC_INTERNAL_H
#define EC_INTERNAL_H

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <ec/platform.h>

#define API_TYPE_PROCESS "Process"
#define API_TYPE_DIRMONITOR "Dirmonitor"

#define API_CONSTANT_DEFINE(L, idx, key, n) (lua_pushnumber(L, n), lua_setfield(L, idx - 1, key))

#endif
