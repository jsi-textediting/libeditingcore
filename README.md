# libeditingcore

A small static C library with the native Lua bindings shared by
[Lite XL](https://github.com/jsi-textediting/lite-xl) and
[thither](https://github.com/jsi-textediting/thither). It has no SDL dependency.

| module | contents |
|---|---|
| `process` | process execution with streamed output (`ec_luaopen_process`) |
| `regex` | PCRE2 regex (`ec_luaopen_regex`) |
| `dirmonitor` | directory watching: inotify, kqueue, fsevents, win32, dummy (`ec_luaopen_dirmonitor`) |
| system core | files, directories, paths, time, environment, fuzzy match, added to the host's `system` table with `ec_system_register(L)` |
| `ec/platform.h` | threads, mutexes, timers, directory listing on POSIX and Win32 |
| `ec/arena_allocator.h`, `ec/utfconv.h` | helpers used by the modules |

## Using it

```cmake
include(FetchContent)
FetchContent_Declare(editingcore
    GIT_REPOSITORY https://github.com/jsi-textediting/libeditingcore.git
    GIT_TAG        v0.1.0)
FetchContent_MakeAvailable(editingcore)
target_link_libraries(myapp PRIVATE editingcore::editingcore)
```

To build against a sibling checkout instead, configure the host with
`-DFETCHCONTENT_SOURCE_DIR_EDITINGCORE=../libeditingcore`.

The host provides Lua and PCRE2 as targets named `lua::lua` and `pcre2-8`. For
standalone builds and tests the library can fetch them itself:

| option | default | |
|---|---|---|
| `EC_FETCH_LUA` | OFF | fetch Lua 5.5 when no `lua::lua` target exists |
| `EC_FETCH_PCRE2` | OFF | fetch PCRE2 when no `pcre2-8` target exists |
| `EC_DIRMONITOR_BACKEND` | auto | `inotify`, `fsevents`, `kqueue`, `win32`, `inodewatcher`, `dummy` |

If a target is missing and its option is OFF, configuration fails and names the option.

## Events

The dirmonitor thread announces changes through hooks the host installs before
opening the module (`include/ec/events.h`):

```c
ec_set_event_hooks(register_fn, push_fn, userdata);
```

An editor turns that into an SDL custom event, a server into a write to a pipe.

## Tests

```
cmake -S . -B build -G Ninja -DEC_FETCH_LUA=ON -DEC_FETCH_PCRE2=ON
cmake --build build && ctest --test-dir build --output-on-failure
```

The Win32 code paths have not been built or run yet.
