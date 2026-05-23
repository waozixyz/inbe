.PHONY: all clean

all: inbe.rom

inbe.rom: inbe.tal
	uxnasm $< $@

clean:
	rm -f inbe.rom inbe.rom.sym
