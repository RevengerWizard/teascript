/*
** tea_debug.c
** Debugging and introspection
*/

#define tea_debug_c
#define TEA_CORE

#include "tea_debug.h"
#include "tea_obj.h"

/* -- Line numbers -------------------------------------------------------- */

/* Get line number for a bytecode position */
int tea_debug_line(GCproto* pt, int pc)
{
    int start = 0;
    int end = pt->line_count - 1;

    while(true)
    {
        int mid = (start + end) / 2;
        LineStart* line = &pt->lines[mid];
        if(pc < line->ofs)
        {
            end = mid - 1;
        }
        else if(mid == pt->line_count - 1 || pc < pt->lines[mid + 1].ofs)
        {
            return line->line;
        }
        else
        {
            start = mid + 1;
        }
    }
}