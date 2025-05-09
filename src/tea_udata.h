/*
** tea_udata.h
** Userdata handling
*/

#ifndef _TEA_UDATA_H
#define _TEA_UDATA_H

#include "tea_obj.h"

TEA_FUNC GCudata* tea_udata_new(tea_State* T, size_t len, uint8_t nuvals);
TEA_FUNC void TEA_FASTCALL tea_udata_free(tea_State* T, GCudata* ud);

#define tea_udata_size(len, uv) (sizeof(struct GCudata) + (len) + ((uv) * sizeof(TValue)))

#endif