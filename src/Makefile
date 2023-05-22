# Teascript Makefile

PLAT = none

CC = gcc
CFLAGS = -O2 $(MYCFLAGS)
AR = ar rcu
RANLIB = ranlib
RM = del
LIBS = -lm $(MYLIBS)

MYCFLAGS =
MYLDFLAGS =
MYLIBS =

# end of user settings

PLATS = mingw

TEA_A = libtea.a
CORE_O = tea_api.o tea_chunk.o tea_compiler.o tea_core.o tea_debug.o \
    tea_do.o tea_gc.o tea_import.o tea_memory.o tea_object.o tea_scanner.o \
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

# Convenience targets for popular platforms

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

tea_api.o: tea_api.c tea.h tea_conf.h tea_state.h tea_common.h \
 tea_value.h tea_array.h tea_object.h tea_memory.h tea_chunk.h \
 tea_opcodes.h tea_table.h tea_vm.h tea_do.h
tea_chunk.o: tea_chunk.c tea_chunk.h tea_common.h tea_value.h tea_array.h \
 tea_opcodes.h tea_memory.h tea_state.h tea.h tea_conf.h tea_object.h \
 tea_table.h tea_vm.h
tea_compiler.o: tea_compiler.c tea_common.h tea_state.h tea.h tea_conf.h \
 tea_value.h tea_array.h tea_object.h tea_memory.h tea_chunk.h \
 tea_opcodes.h tea_table.h tea_compiler.h tea_scanner.h tea_token.h \
 tea_gc.h tea_import.h tea_do.h
tea_core.o: tea_core.c tea.h tea_conf.h tea_vm.h tea_state.h tea_common.h \
 tea_value.h tea_array.h tea_object.h tea_memory.h tea_chunk.h \
 tea_opcodes.h tea_table.h tea_core.h tea_utf.h
tea_debug.o: tea_debug.c tea_debug.h tea_chunk.h tea_common.h tea_value.h \
 tea_array.h tea_opcodes.h tea_object.h tea.h tea_conf.h tea_memory.h \
 tea_table.h tea_state.h
tea_do.o: tea_do.c tea_common.h tea_do.h tea_state.h tea.h tea_conf.h \
 tea_value.h tea_array.h tea_object.h tea_memory.h tea_chunk.h \
 tea_opcodes.h tea_table.h tea_vm.h tea_compiler.h tea_scanner.h \
 tea_token.h tea_debug.h
tea_fileclass.o: tea_fileclass.c tea.h tea_conf.h tea_vm.h tea_state.h \
 tea_common.h tea_value.h tea_array.h tea_object.h tea_memory.h \
 tea_chunk.h tea_opcodes.h tea_table.h tea_core.h
tea_gc.o: tea_gc.c tea_state.h tea.h tea_conf.h tea_common.h tea_value.h \
 tea_array.h tea_object.h tea_memory.h tea_chunk.h tea_opcodes.h \
 tea_table.h tea_gc.h tea_compiler.h tea_scanner.h tea_token.h
tea_import.o: tea_import.c tea.h tea_conf.h tea_state.h tea_common.h \
 tea_value.h tea_array.h tea_object.h tea_memory.h tea_chunk.h \
 tea_opcodes.h tea_table.h tea_import.h
tea_iolib.o: tea_iolib.c tea.h tea_conf.h tea_import.h tea_core.h \
 tea_state.h tea_common.h tea_value.h tea_array.h tea_object.h \
 tea_memory.h tea_chunk.h tea_opcodes.h tea_table.h tea_vm.h
tea_listclass.o: tea_listclass.c tea.h tea_conf.h tea_vm.h tea_state.h \
 tea_common.h tea_value.h tea_array.h tea_object.h tea_memory.h \
 tea_chunk.h tea_opcodes.h tea_table.h tea_core.h
tea_mapclass.o: tea_mapclass.c tea_vm.h tea_state.h tea.h tea_conf.h \
 tea_common.h tea_value.h tea_array.h tea_object.h tea_memory.h \
 tea_chunk.h tea_opcodes.h tea_table.h tea_core.h
tea_mathlib.o: tea_mathlib.c tea.h tea_conf.h tea_import.h tea_core.h \
 tea_state.h tea_common.h tea_value.h tea_array.h tea_object.h \
 tea_memory.h tea_chunk.h tea_opcodes.h tea_table.h
tea_memory.o: tea_memory.c tea.h tea_conf.h tea_common.h tea_memory.h \
 tea_value.h tea_array.h tea_state.h tea_object.h tea_chunk.h \
 tea_opcodes.h tea_table.h
tea_object.o: tea_object.c tea_memory.h tea_value.h tea_common.h \
 tea_array.h tea_object.h tea.h tea_conf.h tea_chunk.h tea_opcodes.h \
 tea_table.h tea_state.h tea_vm.h
tea_oslib.o: tea_oslib.c tea.h tea_conf.h tea_import.h tea_core.h \
 tea_state.h tea_common.h tea_value.h tea_array.h tea_object.h \
 tea_memory.h tea_chunk.h tea_opcodes.h tea_table.h
tea_randomlib.o: tea_randomlib.c tea.h tea_conf.h tea_import.h tea_core.h \
 tea_state.h tea_common.h tea_value.h tea_array.h tea_object.h \
 tea_memory.h tea_chunk.h tea_opcodes.h tea_table.h
tea_rangeclass.o: tea_rangeclass.c tea.h tea_conf.h tea_vm.h tea_state.h \
 tea_common.h tea_value.h tea_array.h tea_object.h tea_memory.h \
 tea_chunk.h tea_opcodes.h tea_table.h tea_core.h
tea_scanner.o: tea_scanner.c tea_common.h tea_value.h tea_array.h \
 tea_scanner.h tea_state.h tea.h tea_conf.h tea_object.h tea_memory.h \
 tea_chunk.h tea_opcodes.h tea_table.h tea_token.h tea_utf.h
tea_state.o: tea_state.c tea_state.h tea.h tea_conf.h tea_common.h \
 tea_value.h tea_array.h tea_object.h tea_memory.h tea_chunk.h \
 tea_opcodes.h tea_table.h tea_core.h tea_vm.h tea_util.h tea_do.h \
 tea_gc.h
tea_stringclass.o: tea_stringclass.c tea.h tea_conf.h tea_vm.h \
 tea_state.h tea_common.h tea_value.h tea_array.h tea_object.h \
 tea_memory.h tea_chunk.h tea_opcodes.h tea_table.h tea_core.h tea_utf.h
tea_syslib.o: tea_syslib.c tea.h tea_conf.h tea_import.h tea_core.h \
 tea_state.h tea_common.h tea_value.h tea_array.h tea_object.h \
 tea_memory.h tea_chunk.h tea_opcodes.h tea_table.h
tea_table.o: tea_table.c tea_gc.h tea_state.h tea.h tea_conf.h \
 tea_common.h tea_value.h tea_array.h tea_object.h tea_memory.h \
 tea_chunk.h tea_opcodes.h tea_table.h
tea_timelib.o: tea_timelib.c tea.h tea_conf.h tea_import.h tea_core.h \
 tea_state.h tea_common.h tea_value.h tea_array.h tea_object.h \
 tea_memory.h tea_chunk.h tea_opcodes.h tea_table.h
tea_utf.o: tea_utf.c tea_utf.h tea_common.h tea_value.h tea_array.h \
 tea_object.h tea.h tea_conf.h tea_memory.h tea_chunk.h tea_opcodes.h \
 tea_table.h
tea_util.o: tea_util.c tea_util.h tea_vm.h tea_state.h tea.h tea_conf.h \
 tea_common.h tea_value.h tea_array.h tea_object.h tea_memory.h \
 tea_chunk.h tea_opcodes.h tea_table.h
tea_value.o: tea_value.c tea_object.h tea.h tea_conf.h tea_common.h \
 tea_memory.h tea_value.h tea_array.h tea_chunk.h tea_opcodes.h \
 tea_table.h
tea_vm.o: tea_vm.c tea_common.h tea_compiler.h tea_scanner.h tea_state.h \
 tea.h tea_conf.h tea_value.h tea_array.h tea_object.h tea_memory.h \
 tea_chunk.h tea_opcodes.h tea_table.h tea_token.h tea_debug.h tea_vm.h \
 tea_util.h tea_utf.h tea_import.h tea_do.h