/*
** tealib.h
** Standard library header
*/

#ifndef _TEALIB_H
#define _TEALIB_H

#include "tea.h"

#define TEA_MODULE_MATH "math"
#define TEA_MODULE_TIME "time"
#define TEA_MODULE_OS "os"
#define TEA_MODULE_SYS "sys"
#define TEA_MODULE_IO "io"
#define TEA_MODULE_RANDOM "random"
#define TEA_MODULE_DEBUG "debug"
#define TEA_MODULE_UTF8 "utf8"

#define TEA_CLASS_LIST "List"
#define TEA_CLASS_MAP "Map"
#define TEA_CLASS_STRING "String"
#define TEA_CLASS_RANGE "Range"
#define TEA_CLASS_BUFFER "Buffer"

TEAMOD_API void tea_import_math(tea_State* T);
TEAMOD_API void tea_import_time(tea_State* T);
TEAMOD_API void tea_import_os(tea_State* T);
TEAMOD_API void tea_import_sys(tea_State* T);
TEAMOD_API void tea_import_io(tea_State* T);
TEAMOD_API void tea_import_random(tea_State* T);
TEAMOD_API void tea_import_debug(tea_State* T);
TEAMOD_API void tea_import_utf8(tea_State* T);

TEAMOD_API void tea_open_list(tea_State* T);
TEAMOD_API void tea_open_map(tea_State* T);
TEAMOD_API void tea_open_string(tea_State* T);
TEAMOD_API void tea_open_range(tea_State* T);
TEAMOD_API void tea_open_buffer(tea_State* T);
TEAMOD_API void tea_open_base(tea_State* T);

#endif