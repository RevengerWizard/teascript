# Teascript Makefile

PLAT = none

CC = gcc -std=gnu99

CCDEBUG =
#CCDEBUG += -O0 -g

CCWARN = -Wall
#CCWARN+= -Wextra

CCOPT = -O2

CFLAGS = $(CCOPT) $(CCWARN) $(SYSCFLAGS) $(MYCFLAGS)
LDFLAGS= $(SYSLDFLAGS) $(MYLDFLAGS)
LIBS = -lm $(SYSLIBS) $(MYLIBS)

AR = ar rcu
RANLIB = ranlib
RM = del

SYSCFLAGS = #-DTEA_DEBUG_PRINT_CODE
SYSLDFLAGS =
SYSLIBS =

MYCFLAGS =
MYLDFLAGS =
MYLIBS =
MYOBJS =

# end of user settings

PLATS = generic linux macosx mingw emscripten

TEA_A = libtea.a
CORE_O = tea_api.o tea_load.o tea_bcwrite.o tea_bcread.o \
	tea_bc.o tea_parse.o tea_debug.o \
    tea_err.o tea_gc.o tea_import.o tea_obj.o tea_func.o tea_map.o tea_str.o tea_lex.o tea_loadlib.o tea_buf.o \
    tea_state.o tea_tab.o tea_utf.o tea_vm.o \
	lib_core.o \
	lib_file.o lib_list.o lib_map.o lib_range.o \
    lib_string.o
LIB_O = lib_io.o lib_os.o lib_random.o lib_math.o \
    lib_sys.o lib_time.o
BASE_O = $(CORE_O) $(LIB_O) $(MYOBJS)

TEA_T = tea
TEA_O = tea.o

ALL_O = $(CORE_O) $(LIB_O) $(TEA_O)
ALL_T = $(TEA_A) $(TEA_T)
ALL_A = $(TEA_A)

Q = @
E = @echo

# Targets start here
default: $(PLAT)

all: $(ALL_T)

o: $(ALL_O)

a: $(ALL_A)

$(TEA_A): $(BASE_O)
	$(AR) $@ $(BASE_O)
	$(RANLIB) $@

$(TEA_T): $(TEA_O) $(TEA_A)
	$(CC) -o $@ $(LDFLAGS) $(TEA_O) $(TEA_A) $(LIBS)

clean:
	$(RM) $(ALL_T) $(ALL_O)

depend:
	@$(CC) $(CFLAGS) -MM tea_*.c > Makefile.dep

echo:
	@echo "PLAT= $(PLAT)"
	@echo "CC= $(CC)"
	@echo "CFLAGS= $(CFLAGS)"
	@echo "LDFLAGS= $(LDFLAGS)"
	@echo "LIBS= $(LIBS)"
	@echo "AR= $(AR)"
	@echo "RANLIB= $(RANLIB)"
	@echo "RM= $(RM)"

# Convenience targets for popular platforms
ALL = all

none:
	@echo "Please choose a platform:"
	@echo "   $(PLATS)"

generic: $(ALL)

onetea:
	$(CC) -O2 -o onetea onetea.c -lm

linux:
	$(MAKE) $(ALL) "SYSCFLAGS=-DTEA_OS=TEA_OS_LINUX -DTEA_USE_READLINE" "SYSLIBS=-Wl,-E -ldl -lreadline"

macosx:
	$(MAKE) $(ALL) "SYSCFLAGS=-DTEA_OS=TEA_OS_MACOSX -DTEA_USE_READLINE" "CC=cc" "SYSLIBS=-lreadline"

mingw:
	$(MAKE) "TEA_A=tea0.dll" "TEA_T=tea.exe" \
	"AR=$(CC) -shared -o" "RANLIB=strip --strip-unneeded" \
	"SYSCFLAGS=-DTEA_OS=TEA_OS_WINDOWS" \
	"MYCFLAGS=-DTEA_BUILD_AS_DLL" "MYLIBS=" "MYLDFLAGS=-s" \ tea.exe

emscripten:
	$(MAKE) "CC=emcc" "AR=emar rcu" "RANLIB=emranlib" \
	"SYSCFLAGS=-DTEA_OS=TEA_OS_OTHER" \
	"CFLAGS=-O2 $(MYCFLAGS) --closure=1" "TEA_A=libtea.bc" "LDFLAGS=" \
	"TEA_T=tea.js" tea.js

.PHONY: all onetea $(PLATS) default o a clean depend echo none

# do not modify

include Makefile.dep