NATIVE_CC ?= gcc

ifeq (, $(shell which $(NATIVE_CC) 2>/dev/null))
$(error "Native GCC is missing. Please install it first. If it's path is custom, set it with export NATIVE_CC=<path to native gcc toolchain>")
endif

.PHONY: all clean

all: pack_assets
	@echo > /dev/null

clean:
	@rm -f pack_assets

pack_assets: pack_assets.c ../lz/lz.c
	@$(NATIVE_CC) -O2 -o $@ pack_assets.c ../lz/lz.c
