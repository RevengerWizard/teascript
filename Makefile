##############################################################################
# Teascript top level Makefile for installation. Requires GNU Make.
#
# Suitable for POSIX platforms (Linux, *BSD, OSX etc.).
# Note: src/Makefile has many more configurable options.
#
# For MSVC, please follow the instructions given in src/msvcbuild.bat
# For MinGW and Cygwin, just run make.
#
# Based upon Makefile of LuaJIT by Mike Pall
##############################################################################

MAJVER = 0
MINVER = 0
ABIVER = 0.0

MMVERSION = $(MAJVER).$(MINVER)
VERSION = $(MMVERSION)

##############################################################################
#
# Change the installation path as needed.
# Note: PREFIX must be an absolute path!
#
export PREFIX = /usr/local
export MULTILIB = lib
##############################################################################

DPREFIX = $(DESTDIR)$(PREFIX)
INSTALL_BIN = $(DPREFIX)/bin
INSTALL_LIB = $(DPREFIX)/$(MULTILIB)
INSTALL_SHARE = $(DPREFIX)/share
INSTALL_DEFINC = $(DPREFIX)/include/tea-$(MMVERSION)
INSTALL_INC = $(INSTALL_DEFINC)

INSTALL_LMODD = $(INSTALL_SHARE)/tea
INSTALL_LMOD = $(INSTALL_LMODD)/$(ABIVER)
INSTALL_CMODD = $(INSTALL_LIB)/tea
INSTALL_CMOD = $(INSTALL_CMODD)/$(ABIVER)
INSTALL_MAN = $(INSTALL_SHARE)/man/man1
INSTALL_PKGCONFIG = $(INSTALL_LIB)/pkgconfig

INSTALL_TNAME = tea-$(VERSION)
INSTALL_TSYMNAME = tea
INSTALL_ANAME = libtea-$(ABIVER).a
INSTALL_SOSHORT1 = libtea-$(ABIVER).so
INSTALL_SOSHORT2 = libtea-$(ABIVER).so.$(MAJVER)
INSTALL_SONAME = libtea-$(ABIVER).so.$(VERSION)
INSTALL_DYLIBSHORT1 = libtea-$(ABIVER).dylib
INSTALL_DYLIBSHORT2 = libtea-$(ABIVER).$(MAJVER).dylib
INSTALL_DYLIBNAME = libtea-$(ABIVER).$(VERSION).dylib
INSTALL_PCNAME = tea.pc

INSTALL_STATIC = $(INSTALL_LIB)/$(INSTALL_ANAME)
INSTALL_DYN = $(INSTALL_LIB)/$(INSTALL_SONAME)
INSTALL_SHORT1 = $(INSTALL_LIB)/$(INSTALL_SOSHORT1)
INSTALL_SHORT2 = $(INSTALL_LIB)/$(INSTALL_SOSHORT2)
INSTALL_T = $(INSTALL_BIN)/$(INSTALL_TNAME)
INSTALL_TSYM = $(INSTALL_BIN)/$(INSTALL_TSYMNAME)
INSTALL_PC = $(INSTALL_PKGCONFIG)/$(INSTALL_PCNAME)

INSTALL_DIRS = $(INSTALL_BIN) $(INSTALL_LIB) $(INSTALL_INC) $(INSTALL_MAN) \
	$(INSTALL_PKGCONFIG) $(INSTALL_LMOD) $(INSTALL_CMOD)
UNINSTALL_DIRS = $(INSTALL_INC) \
	$(INSTALL_LMOD) $(INSTALL_LMODD) $(INSTALL_CMOD) $(INSTALL_CMODD)

RM = rm -f
MKDIR = mkdir -p
RMDIR = rmdir 2>/dev/null
SYMLINK = ln -sf
INSTALL_X = install -m 0755
INSTALL_F = install -m 0644
UNINSTALL = $(RM)
LDCONFIG = ldconfig -n 2>/dev/null
SED_PC = sed -e "s|^prefix=.*|prefix=$(PREFIX)|" \
		-e "s|^multilib=.*|multilib=$(MULTILIB)|" \
		-e "s|^relver=.*|relver=$(RELVER)|"
ifneq ($(INSTALL_DEFINC),$(INSTALL_INC))
	SED_PC += -e "s|^includedir=.*|includedir=$(INSTALL_INC)|"
endif

FILE_T = tea
FILE_A = libtea.a
FILE_SO = libtea.so
FILE_MAN = tea.1
FILE_PC = tea.pc
FILES_INC = tea.h tealib.h teaconf.h tea.hpp

ifeq (,$(findstring Windows,$(OS)))
	HOST_SYS := $(shell uname -s)
else
	HOST_SYS = Windows
endif
TARGET_SYS ?= $(HOST_SYS)

ifeq (Darwin,$(TARGET_SYS))
	INSTALL_SONAME = $(INSTALL_DYLIBNAME)
	INSTALL_SOSHORT1 = $(INSTALL_DYLIBSHORT1)
	INSTALL_SOSHORT2 = $(INSTALL_DYLIBSHORT2)
	LDCONFIG = :
endif

##############################################################################

TEST = python

INSTALL_DEP = src/tea

default all $(INSTALL_DEP):
	@echo "==== Building Teascript ===="
	$(MAKE) -C src
	@echo "==== Successfully built Teascript ===="

install: $(INSTALL_DEP)
	@echo "==== Installing Teascript to $(PREFIX) ===="
	$(MKDIR) $(INSTALL_DIRS)
	cd src && $(INSTALL_X) $(FILE_T) $(INSTALL_T)
	cd src && test -f $(FILE_A) && $(INSTALL_F) $(FILE_A) $(INSTALL_STATIC) || :
	$(RM) $(INSTALL_DYN) $(INSTALL_SHORT1) $(INSTALL_SHORT2)
	cd src && test -f $(FILE_SO) && \
		$(INSTALL_X) $(FILE_SO) $(INSTALL_DYN) && \
		( $(LDCONFIG) $(INSTALL_LIB) || : ) && \
		$(SYMLINK) $(INSTALL_SONAME) $(INSTALL_SHORT1) && \
		$(SYMLINK) $(INSTALL_SONAME) $(INSTALL_SHORT2) || :
	cd etc && $(INSTALL_F) $(FILE_MAN) $(INSTALL_MAN)
	cd etc && $(SED_PC) $(FILE_PC) > $(FILE_PC).tmp && \
		$(INSTALL_F) $(FILE_PC).tmp $(INSTALL_PC) && \
		$(RM) $(FILE_PC).tmp
	cd src && $(INSTALL_F) $(FILES_INC) $(INSTALL_INC)
	$(SYMLINK) $(INSTALL_TNAME) $(INSTALL_TSYM)
	@echo "==== Successfully installed Teascript to $(PREFIX) ===="

uninstall:
	@echo "==== Uninstalling Teascript from $(PREFIX) ===="
	$(UNINSTALL) $(INSTALL_TSYM) $(INSTALL_T) $(INSTALL_STATIC) $(INSTALL_DYN) $(INSTALL_SHORT1) $(INSTALL_SHORT2) $(INSTALL_MAN)/$(FILE_MAN) $(INSTALL_PC)
	for file in $(FILES_INC); do \
		$(UNINSTALL) $(INSTALL_INC)/$$file; \
		done
	$(LDCONFIG) $(INSTALL_LIB)
	$(RMDIR) $(UNINSTALL_DIRS) || :
	@echo "==== Successfully uninstalled Teascript from $(PREFIX) ===="

##############################################################################

clean depend:
	$(MAKE) -C src $@

onetea:
	@echo "==== Building Teascript (amalgamation) ===="
	$(MAKE) -C src onetea
	@echo "==== Successfully built Teascript (amalgamation) ===="

test:
	@$(TEST) util/test.py

.PHONY: all onetea clean depend test

##############################################################################