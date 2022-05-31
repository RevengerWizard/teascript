#ifndef TEA_PREDEFINES_H
#define TEA_PREDEFINES_H

#include "tea_common.h"

typedef struct TeaScanner TeaScanner;
typedef struct TeaCompiler TeaCompiler;
typedef struct TeaVM TeaVM;
typedef struct TeaState TeaState;
typedef enum TeaInterpretResult TeaInterpretResult;

typedef struct TeaObject TeaObject;
typedef struct TeaObjectString TeaObjectString;
typedef struct TeaObjectFile TeaObjectFile;
typedef struct TeaObjectData TeaObjectData;

typedef uint64_t TeaValue;

#endif