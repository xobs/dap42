## Copyright (c) 2016, Devan Lai
##
## Permission to use, copy, modify, and/or distribute this software
## for any purpose with or without fee is hereby granted, provided
## that the above copyright notice and this permission notice
## appear in all copies.
##
## THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
## WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
## WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
## AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
## CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
## LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
## NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
## CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# Be silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q              := @
NULL           := 2>/dev/null
MAKE           := $(MAKE) --no-print-directory
endif
export V

BUILD_DIR      ?= ./build

all: DAP42.bin DAP42DC.bin KITCHEN42.bin \
     DAP103.bin DAP103-DFU.bin \
     DAP103-BLUEPILL.bin DAP103-BLUEPILL-DFU.bin \
     DAP103-HID.bin DAP103-HID-DFU.bin \
     DAP103-HID-BLUEPILL.bin DAP103-HID-BLUEPILL-DFU.bin \
     DAP103-NUCLEO.bin DAP103-NUCLEO-STBOOT.bin \
     BRAINv3.3.bin \
     DAP42K6U.bin TINYDYNE.bin NANODYNE.bin FRDM-IMX93.bin
clean:
	$(Q)$(RM) $(BUILD_DIR)/*.bin
	$(Q)$(RM) -r $(BUILD_DIR)/obj
	$(Q)$(MAKE) -C src/ clean

# `.PHONY` is a special target, not a variable: with `=` this defined a
# variable of that name and marked nothing phony, so `all` and `clean` would
# have been skipped had files of those names ever existed.
.PHONY: all clean

# Each target builds into its own directory under `$(BUILD_DIR)/obj`, so
# several can run at once. They all compile the same source paths to the same
# object names, so when they shared `src/` one target's `clean` deleted
# another's objects mid-compile and both raced to write `src/DAP42.bin` --
# `make -j` produced corrupt or failed builds rather than faster ones.
#
# Separate directories also make the per-target `clean` unnecessary, so an
# incremental rebuild now only recompiles what changed.
#
# $(1) is the value of TARGET, $(2) the output name.
define build_firmware
	@printf "  BUILD $(2)\n"
	$(Q)$(MAKE) TARGET=$(1) OBJDIR=$(CURDIR)/$(BUILD_DIR)/obj/$(basename $(2)) -C src/
	$(Q)cp $(BUILD_DIR)/obj/$(basename $(2))/DAP42.bin $(BUILD_DIR)/$(2)
endef

$(BUILD_DIR):
	$(Q)mkdir -p $(BUILD_DIR)

DAP42.bin: | $(BUILD_DIR)
	$(call build_firmware,STM32F042,$(@))

DAP42DC.bin: | $(BUILD_DIR)
	$(call build_firmware,DAP42DC,$(@))

TINYDYNE.bin: | $(BUILD_DIR)
	$(call build_firmware,TINYDYNE,$(@))

NANODYNE.bin: | $(BUILD_DIR)
	$(call build_firmware,NANODYNE,$(@))

FRDM-IMX93.bin: | $(BUILD_DIR)
	$(call build_firmware,FRDM-IMX93,$(@))

KITCHEN42.bin: | $(BUILD_DIR)
	$(call build_firmware,KITCHEN42,$(@))

DAP103.bin: | $(BUILD_DIR)
	$(call build_firmware,STM32F103,$(@))

DAP103-DFU.bin: | $(BUILD_DIR)
	$(call build_firmware,STM32F103-DFUBOOT,$(@))

DAP103-BLUEPILL.bin: | $(BUILD_DIR)
	$(call build_firmware,STM32F103-BLUEPILL,$(@))

DAP103-BLUEPILL-DFU.bin: | $(BUILD_DIR)
	$(call build_firmware,STM32F103-BLUEPILL-DFUBOOT,$(@))

DAP103-HID.bin: | $(BUILD_DIR)
	$(call build_firmware,STM32F103-HID,$(@))

DAP103-HID-DFU.bin: | $(BUILD_DIR)
	$(call build_firmware,STM32F103-HID-DFUBOOT,$(@))

DAP103-HID-BLUEPILL.bin: | $(BUILD_DIR)
	$(call build_firmware,STM32F103-HID-BLUEPILL,$(@))

DAP103-HID-BLUEPILL-DFU.bin: | $(BUILD_DIR)
	$(call build_firmware,STM32F103-HID-BLUEPILL-DFUBOOT,$(@))

BRAINv3.3.bin: | $(BUILD_DIR)
	$(call build_firmware,BRAINV3.3,$(@))

DAP42K6U.bin: | $(BUILD_DIR)
	$(call build_firmware,DAP42K6U,$(@))

DAP103-NUCLEO.bin: | $(BUILD_DIR)
	$(call build_firmware,STLINKV2-1,$(@))

DAP103-NUCLEO-STBOOT.bin: | $(BUILD_DIR)
	$(call build_firmware,STLINKV2-1-STBOOT,$(@))
