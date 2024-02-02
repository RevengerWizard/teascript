/*
** tea_err.h
** Error handling
*/

#ifndef _TEA_ERR_H
#define _TEA_ERR_H

#include "tea_state.h"
#include "tea_lex.h"

typedef enum
{
#define ERRDEF(name, msg) \
    TEA_ERR_##name, TEA_ERR_##name##_ = TEA_ERR_##name + sizeof(msg)-1,
#include "tea_errmsg.h"
    TEA_ERR__MAX
} ErrMsg;

TEA_DATA const char* tea_err_allmsg;
#define err2msg(em) (tea_err_allmsg+(int)(em))

typedef void (*tea_CPFunction)(tea_State* T, void* ud);

TEA_FUNC void tea_err_run(tea_State* T, const char* format, ...);
TEA_FUNC void tea_err_lex(tea_State* T, const char* src, const char* tok, int line, const char* message);
TEA_FUNC void tea_err_throw(tea_State* T, int code);
TEA_FUNC int tea_err_protected(tea_State* T, tea_CPFunction f, void* ud);

#endif