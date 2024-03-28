##############################################################################
# Teascript top level Makefile for installation. Requires GNU Make.
#
# Suitable for POSIX platforms (Linux, *BSD, OSX etc.).
# Note: src/Makefile has many more configurable options.
#
# ##### This Makefile is NOT useful for Windows! #####
# For MSVC, please follow the instructions given in src/msvcbuild.bat.
# For MinGW and Cygwin, cd to src and run make with the Makefile there.
#
# Based upon Makefile of LuaJIT by Mike Pall
##############################################################################

TEST = python

INSTALL_DEP = src/tea

default all $(INSTALL_DEP):
	@echo "==== Building Teascript ===="
	$(MAKE) -C src
	@echo "==== Successfully built Teascript ===="

clean:
	$(MAKE) -C src $@

onetea:
	$(MAKE) -C src onetea

test:
	@$(TEST) util/test.py

.PHONY: all onetea clean test