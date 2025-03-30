##############################################################################
# Teascript Makefile. Requires GNU Make.
#
# Suitable for POSIX platforms (Linux, *BSD, OSX etc.).
# Also works with MinGW and Cygwin on Windows.
# Please check msvcbuild.bat for building with MSVC on Windows.
#
# Based upon Makefile of LuaJIT by Mike Pall
##############################################################################

MAJVER =  0
MINVER =  0
RELVER =  0
ABIVER =  0.0
NODOTABIVER = 00

##############################################################################
#############################  COMPILER OPTIONS  #############################
##############################################################################
#
DEFAULT_CC = gcc
#
CC = $(DEFAULT_CC)
#
# Use this if you want to force a 32 bit build on a 64 bit multilib OS.
#CC = $(DEFAULT_CC) -m32
#
CCOPT = -O2 #-fomit-frame-pointer
# Use this if you want to generate a smaller binary (but it's slower):
#CCOPT = -Os -fomit-frame-pointer
# Note: it's no longer recommended to use -O3 with GCC 4.x
# The I-Cache bloat usually outweighs the benefits from aggressive inlining.
#
CCDEBUG =
# Uncomment the next line to generate debug information:
#CCDEBUG = -g
#
CCWARN = -Wall
# Uncomment the next line to enable more warnings:
#CCWARN+= -Wextra -Wdeclaration-after-statement -Wredundant-decls -Wshadow -Wpointer-arith
#
##############################################################################

##############################################################################
################################  BUILD MODE  ################################
##############################################################################
# The default build mode is mixed mode on POSIX. On Windows this is the same
# as dynamic mode.
#
# Mixed mode creates a static + dynamic library and a statically linked tea.
BUILDMODE = mixed
#
# Static mode creates a static library and a statically linked tea.
#BUILDMODE = static
#
# Dynamic mode creates a dynamic library and a dynamically linked tea.
# Note: this executable will only run when the library is installed!
#BUILDMODE = dynamic
#
##############################################################################

##############################################################################
#################################  FEATURES  #################################
##############################################################################
# Enable/disable these features as needed, but make sure you force a full
# recompile with "make clean", followed by "make"
XCFLAGS =
#
# No special feature flags as of now :(
#
##############################################################################

##############################################################################
############################  DEBUGGING SUPPORT  #############################
##############################################################################
# Enable these options as needed, but make sure you force a full recompile
# with "make clean", followed by "make".
# Note that most of these are NOT suitable for benchmarking or release mode!
#
# Turn on assertions for the Teascript C API to debug problems with tea_* calls.
# This is rather slow -- use only while developing C libraries/embeddings.
#XCFLAGS += -DTEA_USE_APICHECK
#
# Turn on assertions for the whole Teascript VM. This significantly slows down
# everything. Use only if you suspect a problem with Teascript itself.
#XCFLAGS += -DTEA_USE_ASSERT
#
##############################################################################
# You probably don't need to change anything below this line!
##############################################################################

##############################################################################
# Host system detection
##############################################################################

ifeq (Windows,$(findstring Windows,$(OS))$(MSYSTEM)$(TERM))
	HOST_SYS= Windows
else
	HOST_SYS:= $(shell uname -s)
	ifneq (,$(findstring MINGW,$(HOST_SYS)))
		HOST_SYS= Windows
		HOST_MSYS= mingw
	endif
	ifneq (,$(findstring MSYS,$(HOST_SYS)))
		HOST_SYS= Windows
		HOST_MSYS= mingw
	endif
	ifneq (,$(findstring CYGWIN,$(HOST_SYS)))
		HOST_SYS= Windows
		HOST_MSYS= cygwin
	endif
endif

##############################################################################
# Flags and options for host and target
##############################################################################

# You can override the following variables at the make command line:
#   CC       HOST_CC       STATIC_CC       DYNAMIC_CC
#   CFLAGS   HOST_CFLAGS   TARGET_CFLAGS
#   LDFLAGS  HOST_LDFLAGS  TARGET_LDFLAGS  TARGET_SHLDFLAGS
#   LIBS     HOST_LIBS     TARGET_LIBS
#   CROSS    HOST_SYS      TARGET_SYS      TARGET_FLAGS
#
# Cross-compilation examples:
#   make HOST_CC="gcc -m32" CROSS=i586-mingw32msvc- TARGET_SYS=Windows
#   make HOST_CC="gcc -m32" CROSS=powerpc-linux-gnu-

ASOPTIONS = $(CCOPT) $(CCWARN) $(XCFLAGS) $(CFLAGS)
CCOPTIONS = $(CCDEBUG) $(ASOPTIONS)
LDOPTIONS = $(CCDEBUG) $(LDFLAGS)

HOST_CC = $(CC)
HOST_RM ?= rm -f

HOST_XCFLAGS = -I.
HOST_XLDFLAGS =
HOST_XLIBS =
HOST_ACFLAGS = $(CCOPTIONS) $(HOST_XCFLAGS) $(TARGET_ARCH) $(HOST_CFLAGS)
HOST_ALDFLAGS = $(LDOPTIONS) $(HOST_XLDFLAGS) $(HOST_LDFLAGS)
HOST_ALIBS = $(HOST_XLIBS) $(LIBS) $(HOST_LIBS)

STATIC_CC = $(CROSS)$(CC)
DYNAMIC_CC = $(CROSS)$(CC) -fPIC
TARGET_CC = $(STATIC_CC)
TARGET_STCC = $(STATIC_CC)
TARGET_DYNCC = $(DYNAMIC_CC)
TARGET_LD = $(CROSS)$(CC)
TARGET_AR = $(CROSS)ar rcu
TARGET_STRIP = $(CROSS)strip

TARGET_SONAME = libtea-$(ABIVER).so.$(MAJVER)
TARGET_DYLIBNAME = libtea-$(ABIVER).$(MAJVER).dylib
TARGET_DYLIBPATH = $(TARGET_DYLIBNAME)
TARGET_DLLNAME = tea$(NODOTABIVER).dll
TARGET_DLLDOTANAME = libtea-$(ABIVER).dll.a
TARGET_XSHLDFLAGS = -shared -fPIC -Wl,-soname,$(TARGET_SONAME)
TARGET_DYNXLDOPTS =

TARGET_LFSFLAGS = -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE
TARGET_XCFLAGS = $(TARGET_LFSFLAGS) -U_FORTIFY_SOURCE
TARGET_XLDFLAGS =
TARGET_XLIBS = -lm
TARGET_TCFLAGS = $(CCOPTIONS) $(TARGET_XCFLAGS) $(TARGET_FLAGS) $(TARGET_CFLAGS)
TARGET_ACFLAGS = $(CCOPTIONS) $(TARGET_XCFLAGS) $(TARGET_FLAGS) $(TARGET_CFLAGS)
TARGET_ASFLAGS = $(ASOPTIONS) $(TARGET_XCFLAGS) $(TARGET_FLAGS) $(TARGET_CFLAGS)
TARGET_ALDFLAGS = $(LDOPTIONS) $(TARGET_XLDFLAGS) $(TARGET_FLAGS) $(TARGET_LDFLAGS)
TARGET_ASHLDFLAGS = $(LDOPTIONS) $(TARGET_XSHLDFLAGS) $(TARGET_FLAGS) $(TARGET_SHLDFLAGS)
TARGET_ALIBS = $(TARGET_XLIBS) $(LIBS) $(TARGET_LIBS)

##############################################################################
# Target system detection
##############################################################################

TARGET_SYS ?= $(HOST_SYS)
ifeq (Windows,$(TARGET_SYS))
	TARGET_STRIP += --strip-unneeded
	TARGET_XSHLDFLAGS = -shared -Wl,--out-implib,$(TARGET_DLLDOTANAME)
	TARGET_DYNXLDOPTS =
else
	TARGET_AR += 2>/dev/null
ifeq (,$(shell $(TARGET_CC) -o /dev/null -c -x c /dev/null -fno-stack-protector 2>/dev/null || echo 1))
	TARGET_XCFLAGS += -fno-stack-protector
endif
ifeq (Darwin,$(TARGET_SYS))
	ifeq (,$(MACOSX_DEPLOYMENT_TARGET))
		$(error missing: export MACOSX_DEPLOYMENT_TARGET=XX.YY)
	endif
	TARGET_STRIP += -x
	TARGET_XSHLDFLAGS = -dynamiclib -single_module -undefined dynamic_lookup -fPIC
	TARGET_DYNXLDOPTS =
	TARGET_XSHLDFLAGS += -install_name $(TARGET_DYLIBPATH) -compatibility_version $(MAJVER).$(MINVER) -current_version $(MAJVER).$(MINVER).$(RELVER)
else
ifeq (iOS,$(TARGET_SYS))
	TARGET_STRIP += -x
	TARGET_XSHLDFLAGS = -dynamiclib -single_module -undefined dynamic_lookup -fPIC
	TARGET_DYNXLDOPTS =
	TARGET_XSHLDFLAGS += -install_name $(TARGET_DYLIBPATH) -compatibility_version $(MAJVER).$(MINVER) -current_version $(MAJVER).$(MINVER).$(RELVER)
else
	ifneq (SunOS,$(TARGET_SYS))
		ifneq (PS3,$(TARGET_SYS))
		TARGET_XLDFLAGS += -Wl,-E
		endif
	endif
	ifeq (Linux,$(TARGET_SYS))
		TARGET_XLIBS += -ldl
	endif
	ifeq (GNU/kFreeBSD,$(TARGET_SYS))
		TARGET_XLIBS += -ldl
	endif
endif
endif
endif

ifneq ($(HOST_SYS),$(TARGET_SYS))
	ifeq (Windows,$(TARGET_SYS))
		HOST_XCFLAGS += -DTEA_OS=TEA_OS_WINDOWS
	else
	ifeq (Linux,$(TARGET_SYS))
		HOST_XCFLAGS += -DTEA_OS=TEA_OS_LINUX
	else
	ifeq (Darwin,$(TARGET_SYS))
		HOST_XCFLAGS += -DTEA_OS=TEA_OS_OSX
	else
	ifeq (iOS,$(TARGET_SYS))
		HOST_XCFLAGS += -DTEA_OS=TEA_OS_OSX
	else
		HOST_XCFLAGS += -DTEA_OS=TEA_OS_OTHER
	endif
	endif
	endif
	endif
endif

ifneq (,$(CCDEBUG))
	TARGET_STRIP = @:
endif

##############################################################################
# Files and pathnames
##############################################################################

TEALIB_O = lib_base.o \
	lib_list.o lib_map.o lib_range.o \
	lib_string.o lib_buffer.o \
	lib_io.o lib_os.o lib_random.o lib_math.o \
	lib_sys.o lib_time.o lib_debug.o lib_utf8.o

TEACORE_O = tea_assert.o tea_api.o tea_lib.o tea_prng.o \
	tea_udata.o tea_meta.o \
	tea_load.o tea_bcwrite.o tea_bcread.o \
	tea_bc.o tea_parse.o tea_debug.o \
	tea_err.o tea_gc.o tea_import.o tea_obj.o tea_func.o \
	tea_list.o tea_map.o tea_str.o tea_lex.o tea_buf.o \
	tea_state.o tea_tab.o tea_vm.o \
	tea_char.o tea_strscan.o tea_strfmt.o tea_strfmt_num.o \
	$(TEALIB_O)

TEAVMCORE_O = $(TEACORE_O)
TEAVMCORE_DYNO = $(TEAVMCORE_O:.o=_dyn.o)

TEA_O = tea.o
TEA_A = libtea.a
TEA_SO = libtea.so
TEA_T = tea

ALL_T = $(TEA_T) $(TEA_A) $(TEA_SO) $(HOST_T)
WIN_RM = *.obj *.lib *.exp *.dll *.exe *.manifest *.pdb *.ilk
ALL_RM = $(ALL_T) *.o $(WIN_RM)

##############################################################################
# Build mode handling
##############################################################################

# Mixed mode defaults
TARGET_O = $(TEA_A)
TARGET_T = $(TEA_T) $(TEA_SO)
TARGET_DEP = $(TEA_SO)

ifeq (Windows,$(TARGET_SYS))
	TARGET_DYNCC = $(STATIC_CC)
	TEA_T = tea.exe
	ifeq (cygwin,$(HOST_MSYS))
		TEA_SO = cyg$(TARGET_DLLNAME)
	else
		TEA_SO = $(TARGET_DLLNAME)
	endif
	# Mixed mode is not supported on Windows. And static mode doesn't work well.
	# C modules cannot be loaded, because they bind to tea.dll
	ifneq (static,$(BUILDMODE))
		BUILDMODE = dynamic
		TARGET_XCFLAGS += -DTEA_BUILD_AS_DLL
	endif
endif
ifeq (SunOS,$(TARGET_SYS))
	BUILDMODE = static
endif

ifeq (Windows,$(HOST_SYS))
	ifeq (,$(HOST_MSYS))
		ALL_RM := $(subst /,\,$(ALL_RM))
		HOST_RM = del
	endif
endif

ifeq (static,$(BUILDMODE))
	TARGET_DYNCC = @:
	TARGET_T = $(TEA_T)
	TARGET_DEP =
else
ifeq (dynamic,$(BUILDMODE))
	ifneq (Windows,$(TARGET_SYS))
		TARGET_CC = $(DYNAMIC_CC)
	endif
	TARGET_DYNCC = @:
	TEAVMCORE_DYNO = $(TEAVMCORE_O)
	TARGET_O = $(TEA_SO)
	TARGET_XLDFLAGS += $(TARGET_DYNXLDOPTS)
else
ifeq (Darwin,$(TARGET_SYS))
	TARGET_DYNCC = @:
	TEAVMCORE_DYNO = $(TEAVMCORE_O)
endif
ifeq (iOS,$(TARGET_SYS))
	TARGET_DYNCC = @:
	TEAVMCORE_DYNO = $(TEAVMCORE_O)
endif
endif
endif

Q = @
E = @echo
#Q =
#E = @:

##############################################################################
# Make targets
##############################################################################

default all:	$(TARGET_T)

amalg:
	$(MAKE) all "TEACORE_O=onetea.o"

clean:
	$(HOST_RM) $(ALL_RM)

depend:
	$(Q)$(HOST_CC) $(HOST_ACFLAGS) -MM *.c > Makefile.dep

.PHONY: default all amalg clean depend

##############################################################################
# Object file rules
##############################################################################

%.o: %.c
	$(E) "CC        $@"
	$(Q)$(TARGET_DYNCC) $(TARGET_ACFLAGS) -c -o $(@:.o=_dyn.o) $<
	$(Q)$(TARGET_CC) $(TARGET_ACFLAGS) -c -o $@ $<

$(TEA_O):
	$(E) "CC        $@"
	$(Q)$(TARGET_STCC) $(TARGET_ACFLAGS) -c -o $@ $<

include Makefile.dep

##############################################################################
# Target file rules
##############################################################################

$(TEA_A): $(TEAVMCORE_O)
	$(E) "AR        $@"
	$(Q)$(TARGET_AR) $@ $(TEAVMCORE_O)

# The dependency on _O, but linking with _DYNO is intentional.
$(TEA_SO): $(TEAVMCORE_O)
	$(E) "DYNLINK   $@"
	$(Q)$(TARGET_LD) $(TARGET_ASHLDFLAGS) -o $@ $(TEAVMCORE_DYNO) $(TARGET_ALIBS)
	$(Q)$(TARGET_STRIP) $@

$(TEA_T): $(TARGET_O) $(TEA_O) $(TARGET_DEP)
	$(E) "LINK      $@"
	$(Q)$(TARGET_LD) $(TARGET_ALDFLAGS) -o $@ $(TEA_O) $(TARGET_O) $(TARGET_ALIBS)
	$(Q)$(TARGET_STRIP) $@
	$(E) "OK        Successfully built Teascript"

##############################################################################