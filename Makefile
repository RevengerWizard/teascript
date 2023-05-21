# Teascript Makefile

PLAT = none

PLATS= mingw

all:	$(PLAT)

$(PLATS) clean:
	cd src && $(MAKE) $@

.PHONY: all $(PLATS)