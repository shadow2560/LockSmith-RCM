rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_rules

################################################################################

IPL_LOAD_ADDR := 0x40008000
MAGIC := 0x4C534D54 #"LSMT"
include ./Versions.inc

################################################################################

TARGET := LockSmith-RCM
BUILDDIR := build
OUTPUTDIR := output
SOURCEDIR := source
BDKDIR := bdk
BDKINC := -I./$(BDKDIR)
KEYGENDIR := keygen
KEYGEN := tsec_keygen
KEYGENH := tsec_keygen.h
VPATH = $(dir ./$(SOURCEDIR)/) $(dir $(wildcard ./$(SOURCEDIR)/*/)) $(dir $(wildcard ./$(SOURCEDIR)/*/*/))
VPATH += $(dir $(wildcard ./$(BDKDIR)/)) $(dir $(wildcard ./$(BDKDIR)/*/)) $(dir $(wildcard ./$(BDKDIR)/*/*/))

OBJS =	$(patsubst $(SOURCEDIR)/%.S, $(BUILDDIR)/$(TARGET)/%.o, \
		$(patsubst $(SOURCEDIR)/%.c, $(BUILDDIR)/$(TARGET)/%.o, \
		$(call rwildcard, $(SOURCEDIR), *.S *.c)))
OBJS +=	$(patsubst $(BDKDIR)/%.S, $(BUILDDIR)/$(TARGET)/%.o, \
		$(patsubst $(BDKDIR)/%.c, $(BUILDDIR)/$(TARGET)/%.o, \
		$(call rwildcard, $(BDKDIR), *.S *.c)))

GFX_INC   := '"../$(SOURCEDIR)/gfx/gfx.h"'
FFCFG_INC := '"../$(SOURCEDIR)/libs/fatfs/ffconf.h"'

################################################################################

CUSTOMDEFINES := -DIPL_LOAD_ADDR=$(IPL_LOAD_ADDR) -DLS_MAGIC=$(MAGIC)
CUSTOMDEFINES += -DLS_VER_MJ=$(LSVERSION_MAJOR) -DLS_VER_MN=$(LSVERSION_MINOR) -DLS_VER_HF=$(LSVERSION_BUGFX) -DLS_VER_RL=$(LSVERSION_REL)
CUSTOMDEFINES += -DGFX_INC=$(GFX_INC) -DFFCFG_INC=$(FFCFG_INC)

#CUSTOMDEFINES += -DDEBUG

# UART Logging: Max baudrate 12.5M.
# DEBUG_UART_PORT - 0: UART_A, 1: UART_B, 2: UART_C.
#CUSTOMDEFINES += -DDEBUG_UART_BAUDRATE=115200 -DDEBUG_UART_INVERT=0 -DDEBUG_UART_PORT=0

# BDK defines.
CUSTOMDEFINES += -DBDK_EMUMMC_ENABLE
CUSTOMDEFINES += -DBDK_RESTART_BL_ON_WDT -DBDK_MC_ENABLE_AHB_REDIRECT
CUSTOMDEFINES += -DBDK_MALLOC_NO_DEFRAG
CUSTOMDEFINES += -DBDK_WATCHDOG_FIQ_ENABLE

#TODO: Considering reinstating some of these when pointer warnings have been fixed.
WARNINGS := -Wall -Wsign-compare -Wtype-limits -Wno-array-bounds -Wno-stringop-overread -Wno-stringop-overflow

ARCH := -march=armv4t -mtune=arm7tdmi -mthumb -mthumb-interwork $(WARNINGS)
CFLAGS = $(ARCH) -Os -nostdlib -ffunction-sections -fdata-sections -fomit-frame-pointer -std=gnu11 $(CUSTOMDEFINES) -I$(GEN_DIR)
# CFLAGS += -fno-inline
LDFLAGS = $(ARCH) -nostartfiles -lgcc -Wl,-Map,build/LockSmith-RCM/LockSmith-RCM.map,--cref,--strip-debug,--nmagic,--gc-sections -Xlinker --defsym=IPL_LOAD_ADDR=$(IPL_LOAD_ADDR)

LDRDIR := $(wildcard loader)
TOOLSLZ := $(wildcard tools/lz)
TOOLSB2C := $(wildcard tools/bin2c)
TOOLSPACK := $(wildcard tools/pack_assets)
TOOLS := $(TOOLSLZ) $(TOOLSB2C) $(TOOLSPACK)

# Generated packed assets (messages and NCA DB)
GEN_DIR := $(BUILDDIR)/$(TARGET)/generated
GEN_MSG_H := $(GEN_DIR)/messages_packed.h
GEN_NCA_H := $(GEN_DIR)/fuse_nca_packed.h

# Add generated include directory
CFLAGS += -I$(GEN_DIR)

################################################################################

.PHONY: all clean tools $(LDRDIR) $(TOOLS)

all: $(OUTPUTDIR)/$(TARGET).bin $(LDRDIR)
	@echo "--------------------------------------"
	@echo -n "Uncompr size: "
	$(eval BIN_SIZE = $(shell wc -c < $(OUTPUTDIR)/$(TARGET)_unc.bin))
	@echo $(BIN_SIZE)" Bytes"
	@echo "Uncompr Max:  140288 Bytes + 3 KiB BSS"
	@if [ ${BIN_SIZE} -gt 140288 ]; then echo "\e[1;33mUncompr size exceeds limit!\e[0m"; fi
	@echo -n "Payload size: "
	$(eval BIN_SIZE = $(shell wc -c < $(OUTPUTDIR)/$(TARGET).bin))
	@echo $(BIN_SIZE)" Bytes"
	@echo "Payload Max:  126296 Bytes"
	@if [ ${BIN_SIZE} -gt 126296 ]; then echo "\e[1;33mPayload size exceeds limit!\e[0m"; fi
	@echo "--------------------------------------"

################################################################################
# Host-side helper tools
################################################################################

# Ensure we have concrete file targets so parallel builds (make -j) do not race.
$(TOOLSLZ)/lz77:
	@$(MAKE) --no-print-directory -C $(TOOLSLZ) -$(MAKEFLAGS)

$(TOOLSB2C)/bin2c:
	@$(MAKE) --no-print-directory -C $(TOOLSB2C) -$(MAKEFLAGS)

$(TOOLSPACK)/pack_assets:
	@$(MAKE) --no-print-directory -C $(TOOLSPACK) -$(MAKEFLAGS)

tools: $(TOOLSLZ)/lz77 $(TOOLSB2C)/bin2c $(TOOLSPACK)/pack_assets

################################################################################

clean:
	@rm -rf $(BUILDDIR)
	@rm -rf $(OUTPUTDIR)
	@rm -f $(KEYGENDIR)/$(KEYGENH)
	@rm -f $(LDRDIR)/payload_00.h
	@rm -f $(LDRDIR)/payload_01.h
	@rm -f $(TOOLSPACK)/pack_assets $(TOOLSPACK)/pack_assets.exe
	@rm -f $(TOOLSB2C)/bin2c $(TOOLSB2C)/bin2c.exe
	@rm -f $(TOOLSLZ)/lz77 $(TOOLSLZ)/lz77.exe
	@$(MAKE) --no-print-directory -C $(TOOLSPACK) clean -$(MAKEFLAGS) || true
	@$(MAKE) --no-print-directory -C $(TOOLSB2C) clean -$(MAKEFLAGS) || true
	@$(MAKE) --no-print-directory -C $(TOOLSLZ) clean -$(MAKEFLAGS) || true


$(LDRDIR): $(OUTPUTDIR)/$(TARGET).bin tools
	@$(TOOLSLZ)/lz77 $(OUTPUTDIR)/$(TARGET).bin
	mv $(OUTPUTDIR)/$(TARGET).bin $(OUTPUTDIR)/$(TARGET)_unc.bin
	@mv $(OUTPUTDIR)/$(TARGET).bin.00.lz payload_00
	@mv $(OUTPUTDIR)/$(TARGET).bin.01.lz payload_01
	@$(TOOLSB2C)/bin2c payload_00 > $(LDRDIR)/payload_00.h
	@$(TOOLSB2C)/bin2c payload_01 > $(LDRDIR)/payload_01.h
	@rm payload_00
	@rm payload_01
	@$(MAKE) --no-print-directory -C $@ $(MAKECMDGOALS) -$(MAKEFLAGS) PAYLOAD_NAME=$(TARGET)


# Backwards compatible: building in tools/* directories explicitly still works.
$(TOOLS):
	@$(MAKE) --no-print-directory -C $@ $(MAKECMDGOALS) -$(MAKEFLAGS)


# Generate packed headers (host-side tool). Single rule generates both headers to avoid -j races.
$(GEN_MSG_H) $(GEN_NCA_H): $(SOURCEDIR)/gfx/messages.c $(SOURCEDIR)/gfx/messages.h $(SOURCEDIR)/fuse_check/fuse_check.c | $(TOOLSPACK)/pack_assets
	@mkdir -p "$(GEN_DIR)"
	@$(TOOLSPACK)/pack_assets "$(SOURCEDIR)" "$(GEN_DIR)"

# In parallel builds, many units include messages.h; make that ordering explicit
# so we never compile before the generated headers exist.
$(OBJS): | $(GEN_MSG_H) $(GEN_NCA_H)

# Ensure key objects rebuild if packed assets change.
$(BUILDDIR)/$(TARGET)/gfx/messages.o: $(GEN_MSG_H)
$(BUILDDIR)/$(TARGET)/gfx/display_log_bin.o: $(GEN_MSG_H)
$(BUILDDIR)/$(TARGET)/fuse_check/fuse_check.o: $(GEN_NCA_H)

$(OUTPUTDIR)/$(TARGET).bin: $(BUILDDIR)/$(TARGET)/$(TARGET).elf tools
	@mkdir -p "$(@D)"
	$(OBJCOPY) -S -O binary $< $@

$(BUILDDIR)/$(TARGET)/$(TARGET).elf: $(OBJS)
	@$(CC) $(LDFLAGS) -T $(SOURCEDIR)/link.ld $^ -o $@
	@echo "LockSmith-RCM was built with the following flags:\nCFLAGS:  "$(CFLAGS)"\nLDFLAGS: "$(LDFLAGS)

$(OBJS): | $(KEYGENDIR)

$(KEYGENDIR): tools
	@cd $(KEYGENDIR) && ../$(TOOLSB2C)/bin2c $(KEYGEN) > $(KEYGENH)

$(BUILDDIR)/$(TARGET)/%.o: $(SOURCEDIR)/%.c
	@mkdir -p "$(@D)"
	@echo Building $@
	@$(CC) $(CFLAGS) $(BDKINC) -c $< -o $@

$(BUILDDIR)/$(TARGET)/%.o: $(SOURCEDIR)/%.S
	@mkdir -p "$(@D)"
	@echo Building $@
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/$(TARGET)/%.o: $(BDKDIR)/%.c
	@mkdir -p "$(@D)"
	@echo Building $@
	@$(CC) $(CFLAGS) $(BDKINC) -c $< -o $@

$(BUILDDIR)/$(TARGET)/%.o: $(BDKDIR)/%.S
	@mkdir -p "$(@D)"
	@echo Building $@
	@$(CC) $(CFLAGS) -c $< -o $@
