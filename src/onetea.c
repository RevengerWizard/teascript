/*
** onetea.c
** Teascript core, libraries and interpreter in a single file
**
** gcc -std=c99 -O2 -o onetea onetea.c -lm
*/

#define onetea_c
#define TEA_CORE

#include "tea.h"

#include "tea_def.h"

#undef TEA_FUNC
#undef TEA_DATA
#define TEA_FUNC    static
#define TEA_DATA    /* empty */

#include "tea_bc.c"
#include "tea_api.c"
#include "tea_bcread.c"
#include "tea_bcwrite.c"
#include "tea_load.c"
#include "tea_buf.c"
#include "tea_parse.c"
#include "tea_debug.c"
#include "tea_strscan.c"
#include "tea_strfmt.c"
#include "tea_strfmt_num.c"
#include "tea_err.c"
#include "tea_gc.c"
#include "tea_import.c"
#include "tea_func.c"
#include "tea_str.c"
#include "tea_map.c"
#include "tea_list.c"
#include "tea_obj.c"
#include "tea_lex.c"
#include "tea_state.c"
#include "tea_tab.c"
#include "tea_utf.c"
#include "tea_vm.c"

#include "lib_base.c"
#include "lib_list.c"
#include "lib_map.c"
#include "lib_range.c"
#include "lib_string.c"

#include "lib_io.c"
#include "lib_os.c"
#include "lib_random.c"
#include "lib_math.c"
#include "lib_sys.c"
#include "lib_time.c"

#include "tea.c"