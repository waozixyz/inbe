# Native make shim. The canonical build is the GNU Makefile.
GMAKE ?= gmake

.MAIN: all

all:
	$(GMAKE) -f Makefile $@

.DEFAULT:
	$(GMAKE) -f Makefile $@
