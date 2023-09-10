# Teascript top level Makefile

PLAT = none

PLATS = generic linux macosx mingw emscripten

TEST = python

$(PLATS) clean:
	$(MAKE) -C src $@

all: $(PLAT)

onetea:
	$(MAKE) -C src onetea

none:
	@echo "Please do"
	@echo "   make PLATFORM"
	@echo "where PLATFORM is one of these:"
	@echo "   $(PLATS)"

test:
	@$(TEST) util/test.py

# Echo config parameters
echo:
	@$(MAKE) -C src -s echo

.PHONY: all onetea $(PLATS) clean test echo