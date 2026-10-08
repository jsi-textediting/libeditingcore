/* The part of the Lua "system" module that does not depend on a window system:
** files, directories, paths, time, environment, fuzzy matching. Hosts add it
** to their own system table with ec_system_register(). */
#include "ec_internal.h"
#include <ec/lua_api.h>
#include <ec/utfconv.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
  #include <direct.h>
  #include <windows.h>
  #include <fileapi.h>
  #define fileno _fileno
  #define ftruncate _chsize
#else
  #include <dirent.h>
  #include <unistd.h>
  #ifdef __linux__
    #include <sys/vfs.h>
  #endif
#endif

#ifdef _WIN32
static char *win32_error(DWORD rc) {
  LPSTR message;
  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    rc,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR) &message,
    0,
    NULL
  );

  return message;
}

static void push_win32_error(lua_State *L, DWORD rc) {
  LPSTR message = win32_error(rc);
  lua_pushstring(L, message);
  LocalFree(message);
}
#endif

// removes an empty directory
static int f_rmdir(lua_State *L) {
  lua_pushboolean(L, ec_remove_path(luaL_checkstring(L, 1)));
  if (!lua_toboolean(L, -1)) {
    lua_pushstring(L, ec_get_error());
    return 2;
  }
  return 1;
}


static int f_chdir(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
#ifdef _WIN32
  LPWSTR wpath = utfconv_utf8towc(path);
  if (wpath == NULL) { return luaL_error(L, UTFCONV_ERROR_INVALID_CONVERSION ); }
  int err = _wchdir(wpath);
  free(wpath);
#else
  int err = chdir(path);
#endif
  if (err) { luaL_error(L, "chdir() failed: %s", strerror(errno)); }
  return 0;
}

static ec_enum_result list_dir_enumeration_callback(void *userdata, const char *dirname, const char *fname) {
  (void) dirname;
  lua_State *L = userdata;
  int len = lua_rawlen(L, -1);
  lua_pushstring(L, fname);
  lua_rawseti(L, -2, len + 1);
  return EC_ENUM_CONTINUE;
}

static int f_list_dir(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  lua_newtable(L);
  bool res = ec_enumerate_directory(path, list_dir_enumeration_callback, L);
  if (!res) {
    lua_pushnil(L);
    lua_pushstring(L, ec_get_error());
    return 2;
  }
  return 1;
}


#ifdef _WIN32
  #define realpath(x, y) _wfullpath(y, x, MAX_PATH)
#endif

static int f_absolute_path(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
#ifdef _WIN32
  LPWSTR wpath = utfconv_utf8towc(path);
  if (!wpath) { return 0; }

  LPWSTR wfullpath = realpath(wpath, NULL);
  free(wpath);
  if (!wfullpath) { return 0; }

  char *res = utfconv_wctoutf8(wfullpath);
  free(wfullpath);
#else
  char *res = realpath(path, NULL);
#endif
  if (!res) { return 0; }
  lua_pushstring(L, res);
  free(res);
  return 1;
}


static int f_get_file_info(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);

  lua_newtable(L);
#ifdef _WIN32
  LPWSTR wpath = utfconv_utf8towc(path);
  if (wpath == NULL) {
    lua_pushnil(L); lua_pushstring(L, UTFCONV_ERROR_INVALID_CONVERSION);
    return 2;
  }
  WIN32_FILE_ATTRIBUTE_DATA data;
  if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &data)) {
    free(wpath);
    lua_pushnil(L); push_win32_error(L, GetLastError());
    return 2;
  }
  free(wpath);
  ULARGE_INTEGER large_int = {0};
  #define TICKS_PER_MILISECOND 10000
  #define EPOCH_DIFFERENCE 11644473600000LL
  // https://stackoverflow.com/questions/6161776/convert-windows-filetime-to-second-in-unix-linux
  large_int.HighPart = data.ftLastWriteTime.dwHighDateTime; large_int.LowPart = data.ftLastWriteTime.dwLowDateTime;
  lua_pushnumber(L, (double)((large_int.QuadPart / TICKS_PER_MILISECOND - EPOCH_DIFFERENCE)/1000.0));
  lua_setfield(L, -2, "modified");

  large_int.HighPart = data.nFileSizeHigh; large_int.LowPart = data.nFileSizeLow;
  lua_pushinteger(L, large_int.QuadPart);
  lua_setfield(L, -2, "size");

  lua_pushstring(L, data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? "dir" : "file");
  lua_setfield(L, -2, "type");

  lua_pushboolean(L, data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
  lua_setfield(L, -2, "symlink");
#else
  struct stat s;
  int err = stat(path, &s);
  if (err < 0) {
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    return 2;
  }

  lua_pushinteger(L, s.st_size);
  lua_setfield(L, -2, "size");

  if (S_ISREG(s.st_mode)) {
    lua_pushstring(L, "file");
  } else if (S_ISDIR(s.st_mode)) {
    lua_pushstring(L, "dir");
  } else {
    lua_pushnil(L);
  }
  lua_setfield(L, -2, "type");

  double mtime;
  #if _BSD_SOURCE || _SVID_SOURCE || _XOPEN_SOURCE > 700 || _POSIX_C_SOURCE >= 200809L
    mtime = (double)s.st_mtim.tv_sec + (s.st_mtim.tv_nsec / 1000000000.0);
  #elif __APPLE__
    #if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
      mtime = (double)s.st_mtimespec.tv_sec + (s.st_mtimespec.tv_nsec / 1000000000.0);
    #else
      mtime = (double)s.st_mtime + (s.st_atimensec / 1000000000.0);
    #endif
  #else
    mtime = s.st_mtime;
  #endif
  lua_pushnumber(L, mtime);
  lua_setfield(L, -2, "modified");

  if (S_ISDIR(s.st_mode)) {
    if (lstat(path, &s) == 0) {
      lua_pushboolean(L, S_ISLNK(s.st_mode));
      lua_setfield(L, -2, "symlink");
    }
  }
#endif
  return 1;
}

#if __linux__
// https://man7.org/linux/man-pages/man2/statfs.2.html

struct f_type_names {
  uint32_t magic;
  const char *name;
};

static struct f_type_names fs_names[] = {
  { 0xef53,     "ext2/ext3" },
  { 0x6969,     "nfs"       },
  { 0x65735546, "fuse"      },
  { 0x517b,     "smb"       },
  { 0xfe534d42, "smb2"      },
  { 0x52654973, "reiserfs"  },
  { 0x01021994, "tmpfs"     },
  { 0x858458f6, "ramfs"     },
  { 0x5346544e, "ntfs"      },
  { 0x0,        NULL        },
};

#endif

static int f_get_fs_type(lua_State *L) {
  #if __linux__
    const char *path = luaL_checkstring(L, 1);
    struct statfs buf;
    int status = statfs(path, &buf);
    if (status != 0) {
      return luaL_error(L, "error calling statfs on %s", path);
    }
    for (int i = 0; fs_names[i].magic; i++) {
      if (fs_names[i].magic == buf.f_type) {
        lua_pushstring(L, fs_names[i].name);
        return 1;
      }
    }
  #endif
  lua_pushstring(L, "unknown");
  return 1;
}


static int f_ftruncate(lua_State *L) {
#if LUA_VERSION_NUM < 503
  // note: it is possible to support pre 5.3 and JIT
  //       since file handles are just FILE*  wrapped in a userdata;
  //       but it is not standardized. YMMV.
  #error luaL_Stream is not supported in this version of Lua.
#endif
  luaL_Stream *stream = luaL_checkudata(L, 1, LUA_FILEHANDLE);
  lua_Integer len = luaL_optinteger(L, 2, 0);
  if (ftruncate(fileno(stream->f), len) != 0) {
    lua_pushboolean(L, 0);
    lua_pushfstring(L, "ftruncate(): %s", strerror(errno));
    return 2;
  }

  lua_pushboolean(L, 1);
  return 1;
}


static int f_mkdir(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);

#ifdef _WIN32
  LPWSTR wpath = utfconv_utf8towc(path);
  if (wpath == NULL) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, UTFCONV_ERROR_INVALID_CONVERSION);
    return 2;
  }

  int err = _wmkdir(wpath);
  free(wpath);
#else
  int err = mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
#endif
  if (err < 0) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, strerror(errno));
    return 2;
  }

  lua_pushboolean(L, 1);
  return 1;
}

static int f_get_process_id(lua_State *L) {
#ifdef _WIN32
  lua_pushinteger(L, GetCurrentProcessId());
#else
  lua_pushinteger(L, getpid());
#endif
  return 1;
}


static int f_get_time(lua_State *L) {
  double n = ec_time_seconds();
  lua_pushnumber(L, n);
  return 1;
}


static int f_sleep(lua_State *L) {
  double n = luaL_checknumber(L, 1);
  if (n < 0) n = 0;
  ec_sleep_ms(n * 1000);
  return 0;
}


static int f_exec(lua_State *L) {
  size_t len;
  const char *cmd = luaL_checklstring(L, 1, &len);
  char *buf = malloc(len + 32);
  if (!buf) { luaL_error(L, "buffer allocation failed"); }
#if _WIN32
  sprintf(buf, "cmd /c \"%s\"", cmd);
  WinExec(buf, SW_HIDE);
#else
  sprintf(buf, "%s &", cmd);
  int res = system(buf);
  (void) res;
#endif
  free(buf);
  return 0;
}

static int f_fuzzy_match(lua_State *L) {
  size_t strLen, ptnLen;
  const char *str = luaL_checklstring(L, 1, &strLen);
  const char *ptn = luaL_checklstring(L, 2, &ptnLen);
  // If true match things *backwards*. This allows for better matching on filenames than the above
  // function. For example, in the lite project, opening "renderer" has lib/font_render/build.sh
  // as the first result, rather than src/renderer.c. Clearly that's wrong.
  bool files = lua_gettop(L) > 2 && lua_isboolean(L,3) && lua_toboolean(L, 3);
  int score = 0, run = 0, increment = files ? -1 : 1;
  const char* strTarget = files ? str + strLen - 1 : str;
  const char* ptnTarget = files ? ptn + ptnLen - 1 : ptn;
  while (strTarget >= str && ptnTarget >= ptn && *strTarget && *ptnTarget) {
    while (strTarget >= str && *strTarget == ' ') { strTarget += increment; }
    while (ptnTarget >= ptn && *ptnTarget == ' ') { ptnTarget += increment; }
    if (tolower(*strTarget) == tolower(*ptnTarget)) {
      score += run * 10 - (*strTarget != *ptnTarget);
      run++;
      ptnTarget += increment;
    } else {
      score -= 10;
      run = 0;
    }
    strTarget += increment;
  }
  if (ptnTarget >= ptn && *ptnTarget) { return 0; }
  lua_pushinteger(L, score - (int)strLen * 10);
  return 1;
}

#ifdef _WIN32
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

/* Special purpose filepath compare function. Corresponds to the
   order used in the TreeView view of the project's files. Returns true if
   path1 < path2 in the TreeView order. */
static int f_path_compare(lua_State *L) {
  size_t len1, len2;
  const char *path1 = luaL_checklstring(L, 1, &len1);
  const char *type1_s = luaL_checkstring(L, 2);
  const char *path2 = luaL_checklstring(L, 3, &len2);
  const char *type2_s = luaL_checkstring(L, 4);
  int type1 = strcmp(type1_s, "dir") != 0;
  int type2 = strcmp(type2_s, "dir") != 0;
  /* Find the index of the common part of the path. */
  size_t offset = 0, i, j;
  for (i = 0; i < len1 && i < len2; i++) {
    if (path1[i] != path2[i]) break;
    if (path1[i] == PATHSEP) {
      offset = i + 1;
    }
  }
  /* If a path separator is present in the name after the common part we consider
     the entry like a directory. */
  if (strchr(path1 + offset, PATHSEP)) {
    type1 = 0;
  }
  if (strchr(path2 + offset, PATHSEP)) {
    type2 = 0;
  }
  /* If types are different "dir" types comes before "file" types. */
  if (type1 != type2) {
    lua_pushboolean(L, type1 < type2);
    return 1;
  }
  /* If types are the same compare the files' path alphabetically. */
  int cfr = -1;
  bool same_len = len1 == len2;
  for (i = offset, j = offset; i <= len1 && j <= len2; i++, j++) {
    if (path1[i] == 0 || path2[j] == 0) {
      if (cfr < 0) cfr = 0; // The strings are equal
      if (!same_len) {
        cfr = (path1[i] == 0);
      }
    } else if (isdigit(path1[i]) && isdigit(path2[j])) {
      size_t ii = 0, ij = 0;
      while (isdigit(path1[i+ii])) { ii++; }
      while (isdigit(path2[j+ij])) { ij++; }

      size_t di = 0, dj = 0;
      for (size_t ai = 0; ai < ii; ++ai) {
        di = di * 10 + (path1[i+ai] - '0');
      }
      for (size_t aj = 0; aj < ij; ++aj) {
        dj = dj * 10 + (path2[j+aj] - '0');
      }

      if (di == dj) {
        continue;
      }
      cfr = (di < dj);
    } else if (path1[i] == path2[j]) {
      continue;
    } else if (path1[i] == PATHSEP || path2[j] == PATHSEP) {
      /* For comparison we treat PATHSEP as if it was the string terminator. */
      cfr = (path1[i] == PATHSEP);
    } else {
      char a = path1[i], b = path2[j];
      if (a >= 'A' && a <= 'Z') a += 32;
      if (b >= 'A' && b <= 'Z') b += 32;
      if (a == b) {
        /* If the strings have the same length, we need
           to keep the first case sensitive difference. */
        if (same_len && cfr < 0) {
          /* Give priority to lower-case characters */
          cfr = (path1[i] > path2[j]);
        }
        continue;
      }
      cfr = (a < b);
    }
    break;
  }
  lua_pushboolean(L, cfr);
  return 1;
}

static int f_setenv(lua_State* L) {
  const char *key = luaL_checkstring(L, 1);
  const char *val = luaL_checkstring(L, 2);
  // right now we overwrite unconditionally
  lua_pushboolean(L, ec_setenv(key, val, 1) == 0);
  return 1;
}


static const luaL_Reg system_core_lib[] = {
  { "rmdir",                 f_rmdir                 },
  { "chdir",                 f_chdir                 },
  { "mkdir",                 f_mkdir                 },
  { "list_dir",              f_list_dir              },
  { "absolute_path",         f_absolute_path         },
  { "get_file_info",         f_get_file_info         },
  { "get_process_id",        f_get_process_id        },
  { "get_time",              f_get_time              },
  { "sleep",                 f_sleep                 },
  { "exec",                  f_exec                  },
  { "fuzzy_match",           f_fuzzy_match           },
  { "path_compare",          f_path_compare          },
  { "get_fs_type",           f_get_fs_type           },
  { "setenv",                f_setenv                },
  { "ftruncate",             f_ftruncate             },
  { NULL, NULL }
};

void ec_system_register(lua_State *L) {
  luaL_setfuncs(L, system_core_lib, 0);
}
