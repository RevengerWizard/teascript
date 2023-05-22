# Teascript Makefile

PLAT = none

PLATS= mingw

all:	$(PLAT)

$(PLATS) clean:
	cd src && $(MAKE) $@

none:
	@echo "Please do"
	@echo "   make PLATFORM"
	@echo "where PLATFORM is one of these:"
	@echo "   $(PLATS)"

.PHONY: all $(PLATS)