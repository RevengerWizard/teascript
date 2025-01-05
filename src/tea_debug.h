/*
** tea_debug.h
** Teascript debug functions
*/

#ifndef _TEA_DEBUG_H
#define _TEA_DEBUG_H

#include "tea_obj.h"

TEA_FUNC BCLine tea_debug_line(GCproto* pt, BCPos pc);
TEA_FUNC void tea_debug_stacktrace(tea_State* T, GCstr* msg);

#endif