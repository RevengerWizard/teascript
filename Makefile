# Teascript top level Makefile

PLAT = none

PLATS = generic linux macosx mingw emscripten

$(PLATS) all clean:
	$(MAKE) -C src $@

none:
	@echo "Please do"
	@echo "   make PLATFORM"
	@echo "where PLATFORM is one of these:"
	@echo "   $(PLATS)"

.PHONY: all $(PLATS) clean