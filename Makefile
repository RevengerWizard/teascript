# Teascript Makefile

PLAT = none

PLATS = mingw

all:	$(PLAT)

$(PLATS) clean:
	$(MAKE) -C src $@

%:
	$(MAKE) -C src $(MAKECMDGOALS)

none:
	@echo "Please do"
	@echo "   make PLATFORM"
	@echo "where PLATFORM is one of these:"
	@echo "   $(PLATS)"

.PHONY: all $(PLATS)