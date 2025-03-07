/*
** tea_errmsg.h
** VM error messages
*/

/* This file may be included multiple times with different ERRDEF macros */

/* Basic error handling */
#include "tea_err.h"
ERRDEF(MEM, "Not enough memory")
ERRDEF(ERRERR, "Error in error handling")

/* Allocations */
ERRDEF(STROV, "String length overflow")
ERRDEF(LISTOV, "List items overflow")
ERRDEF(STKOV, "Stack overflow")

/* Map indexing */
ERRDEF(NILIDX, "Map index is nil")
ERRDEF(NANIDX, "Map index is nan")
ERRDEF(NEXTIDX, "Invalid key to " TEA_QL("next"))

/* Arguments errors */
ERRDEF(ARGS, "Expected %d arguments, but got %d")
ERRDEF(NOVAL, "Expected value")
ERRDEF(BADTYPE, "Expected %s, got %s")
ERRDEF(BADARG, "Bad argument %d, %s")
ERRDEF(INTRANGE, "Number out of range")

/* Path errors */
ERRDEF(PATH, "Unable to resolve path " TEA_QS)
ERRDEF(NOPATH, "Could not resolve path " TEA_QS)

/* String buffer errors */
ERRDEF(BUFFER_SELF, "Cannot put buffer into itself")

/* VM errors */
ERRDEF(TOSTR,  TEA_QL("tostring") " must return a string")
ERRDEF(CALL, TEA_QS " is not callable")
ERRDEF(METHOD, "Undefined method " TEA_QS)
ERRDEF(MODVAR, "Undefined variable " TEA_QS " in " TEA_QS " module")
ERRDEF(NOMETHOD, TEA_QS " has no method " TEA_QS)
ERRDEF(SUBSCR, TEA_QS " is not subscriptable")
ERRDEF(INSTSUBSCR, TEA_QS " instance is not subscriptable")
ERRDEF(NUMRANGE, "Range index must be a number")
ERRDEF(IDXRANGE, "Range index out of bounds")
ERRDEF(NUMLIST, "List index must be a number")
ERRDEF(IDXLIST, "List index out of bounds")
ERRDEF(MAPKEY, "Key does not exist within map")
ERRDEF(NUMSTR, "String index must be a number, got " TEA_QS)
ERRDEF(IDXSTR, "String index out of bounds")
ERRDEF(SETSUBSCR, TEA_QS " does not support item assignment")
ERRDEF(MODATTR, TEA_QS " module has no property: " TEA_QS)
ERRDEF(MAPATTR, "Map has no property: " TEA_QS)
ERRDEF(NOATTR, TEA_QS " has no property " TEA_QS)
ERRDEF(SETATTR, "Cannot set property on type " TEA_QS)
ERRDEF(UNOP, "Attempt to use " TEA_QS " unary operator with " TEA_QS)
ERRDEF(BIOP, "Attempt to use " TEA_QS " operator with " TEA_QS " and " TEA_QS)
ERRDEF(RANGE, "Range operands must be numbers")
ERRDEF(UNPACK, "Can only unpack lists")
ERRDEF(MAXUNPACK, "Too many values to unpack")
ERRDEF(MINUNPACK, "Not enough values to unpack")
ERRDEF(SUPER, "Superclass must be a class")
ERRDEF(IS, "Right operand must be a class")
ERRDEF(ITER, TEA_QS " is not iterable")
ERRDEF(BUILTINSELF, "Cannot inherit from built-in " TEA_QS)
ERRDEF(SELF, "A class can't inherit from itself")
ERRDEF(ISCLASS, "Expected class, got " TEA_QS)
ERRDEF(VARMOD, TEA_QS " variable can't be found in module " TEA_QS)
ERRDEF(NONEW, TEA_QS " class has no constructor " TEA_QL("new"))

/* Standard library function errors */
ERRDEF(ASSERT, "Assertion failed")
ERRDEF(OPEN, "Unable to open file " TEA_QS)
ERRDEF(DUMP, "Unable to dump given function")
ERRDEF(STRFMT, "Invalid option " TEA_QS " to " TEA_QL("format"))

/* Bytecode reader errors */
ERRDEF(BCFMT, "Cannot load incompatible bytecode")
ERRDEF(BCBAD, "Cannot load malformed bytecode")

/* Lexer/parser errors */
ERRDEF(XMODE, "Attempt to load code with wrong mode")
ERRDEF(XNEAR, "%s near " TEA_QS)
ERRDEF(XNUMBER, "Malformed number")
ERRDEF(XLEVELS, "Too many syntax levels")
ERRDEF(XSFMT, "String interpolation too deep")
ERRDEF(XSTR, "Unterminated string")
ERRDEF(XHESC, "Incomplete hex escape sequence")
ERRDEF(XUESC, "Incomplete unicode escape sequence")
ERRDEF(XESC, "Invalid escape character")
ERRDEF(XLCOM, "Unterminated block comment")
ERRDEF(XCHAR, "Unexpected character")
ERRDEF(XLOOP, "Loop body too big")
ERRDEF(XKCONST, "Too many constants in one chunk")
ERRDEF(XLINES, "Too many lines in one chunk")
ERRDEF(XJUMP, "Too much code to jump over")
ERRDEF(XLIMM, "Main function has more than %d %s")
ERRDEF(XLIMF, "Function at line %d has more than %d %s")
ERRDEF(XARGS, "Can't have more than 255 arguments")
ERRDEF(XVCONST, "Cannot assign to a const variable")
ERRDEF(XSUPERO, "Can't use " TEA_QL("super") " outside of a class")
ERRDEF(XSUPERK, "Can't use " TEA_QL("super") " in a class with no superclass")
ERRDEF(XSELFO, "Can't use " TEA_QL("self") " outside of a class")
ERRDEF(XSELFS, "Can't use " TEA_QL("self") " inside a static method")
ERRDEF(XVAR, "Undefined variable " TEA_QS)
ERRDEF(XASSIGN, "Invalid assignment target")
ERRDEF(XEXPR, "Expected expression")
ERRDEF(XDUPARGS, "Duplicate parameter name in function declaration")
ERRDEF(XSPREADARGS, "Spread parameter must be last in the parameter list")
ERRDEF(XSPREADOPT, "Spread parameter cannot have an optional value")
ERRDEF(XOPT, "Cannot have non-optional parameter after optional")
ERRDEF(XMAXARGS, "Cannot have more than 255 parameters")
ERRDEF(XDECL, "Variable " TEA_QS " was already declared in this scope")
ERRDEF(XMETHOD, "Invalid method name")
ERRDEF(XSINGLEREST, "Cannot rest single variable")
ERRDEF(XVALASSIGN, "Not enough values to assign to")
ERRDEF(XBREAK, "Cannot use 'break' outside of a loop")
ERRDEF(XCONTINUE, "Cannot use 'continue' outside of a loop")
ERRDEF(XCASE, "Unexpected case after default")
ERRDEF(XRET, "Can't return from top-level code")
ERRDEF(XINIT, "Can't return a value from init")
ERRDEF(XTOKEN, "Expected " TEA_QS)
ERRDEF(XDOTS, "Multiple " TEA_QL("..."))
ERRDEF(XSWITCH, "Switch statement can not have more than 256 case blocks")
ERRDEF(XVASSIGN, "Not enough values to assign to")

#undef ERRDEF

/* Detecting unused error messages:
   awk -F, '/^ERRDEF/ { gsub(/ERRDEF./, ""); printf "grep -q TEA_ERR_%s *.[ch] || echo %s\n", $1, $1}' tea_errmsg.h | sh
*/