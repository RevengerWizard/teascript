/*
** tealib.h
** Standard library header
*/

#ifndef _TEALIB_H
#define _TEALIB_H

#include "tea.h"

#define TEA_MODULE_MATH "math"
TEAMOD_API void tea_import_math(tea_State* T);

#define TEA_MODULE_TIME "time"
TEAMOD_API void tea_import_time(tea_State* T);

#define TEA_MODULE_OS "os"
TEAMOD_API void tea_import_os(tea_State* T);

#define TEA_MODULE_SYS "sys"
TEAMOD_API void tea_import_sys(tea_State* T);

#define TEA_MODULE_IO "io"
TEAMOD_API void tea_import_io(tea_State* T);

#define TEA_MODULE_RANDOM "random"
TEAMOD_API void tea_import_random(tea_State* T);

#define TEA_MODULE_DEBUG "debug"
TEAMOD_API void tea_import_debug(tea_State* T);

#define TEA_CLASS_LIST "List"
TEAMOD_API void tea_open_list(tea_State* T);

#define TEA_CLASS_MAP "Map"
TEAMOD_API void tea_open_map(tea_State* T);

#define TEA_CLASS_STRING "String"
TEAMOD_API void tea_open_string(tea_State* T);

#define TEA_CLASS_RANGE "Range"
TEAMOD_API void tea_open_range(tea_State* T);

#define TEA_CLASS_BUFFER "Buffer"
TEAMOD_API void tea_open_buffer(tea_State* T);

TEAMOD_API void tea_open_base(tea_State* T);

#endif