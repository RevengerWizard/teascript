#ifndef TEALIB_H
#define TEALIB_H

#include "tea.h"

#define TEA_MATH_MODULE "math"
TEAMOD_API void tea_import_math(TeaState* T);

#define TEA_TIME_MODULE "time"
TEAMOD_API void tea_import_time(TeaState* T);

#define TEA_OS_MODULE "os"
TEAMOD_API void tea_import_os(TeaState* T);

#define TEA_SYS_MODULE "sys"
TEAMOD_API void tea_import_sys(TeaState* T);

#define TEA_IO_MODULE "io"
TEAMOD_API void tea_import_io(TeaState* T);

#define TEA_RANDOM_MODULE "random"
TEAMOD_API void tea_import_random(TeaState* T);

#endif