# Teascript top level Makefile

PLAT = none

PLATS = generic linux macosx mingw

$(PLATS) clean:
	$(MAKE) -C src $@

none:
	@echo "Please do"
	@echo "   make PLATFORM"
	@echo "where PLATFORM is one of these:"
	@echo "   $(PLATS)"

.PHONY: $(PLATS) clean