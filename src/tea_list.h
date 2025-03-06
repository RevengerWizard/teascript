/*
** tea_list.h
** List handling
*/

#ifndef _TEA_LIST_H
#define _TEA_LIST_H

#include "tea_obj.h"

TEA_FUNC GClist* tea_list_new(tea_State* T, size_t n);
TEA_FUNC void TEA_FASTCALL tea_list_free(tea_State* T, GClist* list);
TEA_FUNC GClist* tea_list_copy(tea_State* T, GClist* list);
TEA_FUNC void tea_list_add(tea_State* T, GClist* list, cTValue* o);
TEA_FUNC void tea_list_addn(tea_State* T, GClist* list, double n);
TEA_FUNC void tea_list_insert(tea_State* T, GClist* list, cTValue* o, int32_t idx);
TEA_FUNC void tea_list_delete(tea_State* T, GClist* list, int32_t idx);
TEA_FUNC GClist* tea_list_slice(tea_State* T, GClist* list, GCrange* range);

#define tea_list_clear(l) ((l)->len = 0)

#endif