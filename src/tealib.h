#ifndef TEALIB_H
#define TEALIB_H

#include "tea.h"

#define TEA_MODULE_MATH "math"
TEAMOD_API void tea_import_math(TeaState* T);

#define TEA_MODULE_TIME "time"
TEAMOD_API void tea_import_time(TeaState* T);

#define TEA_MODULE_OS "os"
TEAMOD_API void tea_import_os(TeaState* T);

#define TEA_MODULE_SYS "sys"
TEAMOD_API void tea_import_sys(TeaState* T);

#define TEA_MODULE_IO "io"
TEAMOD_API void tea_import_io(TeaState* T);

#define TEA_MODULE_RANDOM "random"
TEAMOD_API void tea_import_random(TeaState* T);

#endif