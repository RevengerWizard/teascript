# Teascript Makefile

PLAT = none

CC = gcc
CFLAGS = -O2 $(MYCFLAGS)
AR = ar rcu
RANLIB = ranlib
RM = del
LIBS = -lm $(MYLIBS)

SYSCFLAGS =
SYSLDFLAGS =
SYSLIBS =

MYCFLAGS =
MYLDFLAGS =
MYLIBS =

# end of user settings

PLATS = mingw

TEA_A = libtea.a
CORE_O = tea_api.o tea_chunk.o tea_compiler.o tea_core.o tea_debug.o \
    tea_do.o tea_gc.o tea_import.o tea_memory.o tea_object.o tea_map.o tea_scanner.o \
    tea_state.o tea_table.o tea_utf.o tea_util.o tea_value.o tea_vm.o
LIB_O = tea_fileclass.o tea_listclass.o tea_mapclass.o tea_rangeclass.o \
    tea_stringclass.o tea_iolib.o tea_oslib.o tea_randomlib.o tea_mathlib.o \
    tea_syslib.o tea_timelib.o

TEA_T = tea
TEA_O = tea.o

ALL_O = $(CORE_O) $(LIB_O) $(TEA_O)
ALL_T = $(TEA_A) $(TEA_T)
ALL_A = $(TEA_A)

default: $(PLAT)

all: $(ALL_T)

o: $(ALL_O)

a: $(ALL_A)

$(TEA_A): $(CORE_O) $(LIB_O)
	$(AR) $@ $(CORE_O) $(LIB_O)
	$(RANLIB) $@

$(TEA_T): $(TEA_O) $(TEA_A)
	$(CC) -o $@ $(MYLDFLAGS) $(TEA_O) $(TEA_A) $(LIBS)

clean:
	$(RM) $(ALL_T) $(ALL_O)

depend:
	@$(CC) $(CFLAGS) -MM tea_*.c > Makefile.dep

# Convenience targets for popular platforms
ALL = all

dev:
	$(MAKE) $(ALL_O)
	$(MAKE) $(TEA_T)

none:
	@echo "Please choose a platform:"
	@echo "   $(PLATS)"

mingw:
	$(MAKE) "TEA_A=tea.dll" "TEA_T=tea.exe" \
	"AR=$(CC) -shared -o" "RANLIB=strip --strip-unneeded" \
	"MYCFLAGS=-DTEA_BUILD_AS_DLL" "MYLIBS=" "MYLDFLAGS=-s" tea.exe
	$(MAKE) a

.PHONY: all $(PLATS) default o a clean depend echo none

# do not modify

include Makefile.dep