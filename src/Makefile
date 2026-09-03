## Copyright (c) 2015, Devan Lai
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

-include local.mk

BINARY          = DAP42
OPENCM3_DIR     = ../libopencm3

TARGET         ?= STM32F042
include targets.mk

DFU_UTIL       ?= dfu-util
DFUSE_VID_PID  := 0483:df11
DAP42_VID_PID  := 1209:da42
DFU_VID_PID    ?= $(DFUSE_VID_PID)
APP_VID_PID    ?= $(DAP42_VID_PID)
# Where objects and the linked output go.
#
# Unset, everything lands beside the sources exactly as it always has, so a
# bare `make -C src/` is unchanged. Set, each caller gets its own tree, which
# is what lets the top-level Makefile build several targets at once: they all
# compile the same source paths to the same object names, so sharing one
# directory means one target's output overwrites another's.
OBJDIR         ?=
ifneq ($(OBJDIR),)
OBJPREFIX      := $(OBJDIR)/
endif

.DEFAULT_GOAL  := $(OBJPREFIX)$(BINARY).bin

ifeq ($(ARCH),STM32F0)
	DFUSE_SERIAL := FFFFFFFEFFFF
endif

ifneq ($(and $(SERIAL),$(DFUSE_SERIAL)),)
	DFU_FLAGS ?= -S $(SERIAL),$(DFUSE_SERIAL)
else ifneq ($(SERIAL),)
	DFU_FLAGS ?= -S $(SERIAL)
endif

%.dfu-flash: %.bin
	$(DFU_UTIL) -d $(APP_VID_PID),$(DFU_VID_PID) $(DFU_FLAGS) -D $(<)

%.dfuse-flash: %.bin
	$(DFU_UTIL) -d $(APP_VID_PID),$(DFUSE_VID_PID) $(DFU_FLAGS) -a 0 -s 0x08000000:leave -D $(<)

dfuse-flash: $(OBJPREFIX)$(BINARY).dfuse-flash

dfu-flash: $(OBJPREFIX)$(BINARY).dfu-flash

size: $(OBJS) $(OBJPREFIX)$(BINARY).elf
	@$(PREFIX)size $(OBJS) $(OBJPREFIX)$(BINARY).elf

debug: $(OBJPREFIX)$(BINARY).elf
	-$(GDB) --tui --eval "target remote | $(OOCD) -f $(OOCD_INTERFACE) -f $(OOCD_BOARD) -f ../openocd/debug.cfg" $(OBJPREFIX)$(BINARY).elf

reset:
	$(OOCD) -f $(OOCD_INTERFACE) -f $(OOCD_BOARD) -c "init; reset halt; reset; shutdown"

clean::
	@rm -f $(OBJS)
	@rm -f $(DEPS)

SRCS           := $(wildcard *.c)
SRCS           += $(wildcard DAP/*.c)
SRCS           += $(wildcard USB/*.c)
SRCS           += $(wildcard DFU/*.c)
SRCS           += $(wildcard CAN/*.c)
SRCS           += $(wildcard $(TARGET_COMMON_DIR)/*.c)
SRCS           += $(wildcard $(TARGET_COMMON_DIR)/DAP/*.c)
SRCS           += $(wildcard $(TARGET_COMMON_DIR)/USB/*.c)
SRCS           += $(wildcard $(TARGET_COMMON_DIR)/DFU/*.c)
SRCS           += $(wildcard $(TARGET_SPEC_DIR)/*.c)
SRCS           += $(wildcard $(TARGET_SPEC_DIR)/DAP/*.c)
SRCS           += $(wildcard $(TARGET_SPEC_DIR)/USB/*.c)
SRCS           += $(wildcard $(TARGET_SPEC_DIR)/DFU/*.c)

OBJS           += $(addprefix $(OBJPREFIX),$(SRCS:.c=.o))
DEPS            = $(OBJS:.o=.d)

include rules.mk

OBJS           := $(sort $(OBJS))

# Add the base directory to the header search path
CPPFLAGS       += -I.

# Add target config directory to the header search path
CPPFLAGS       += -I$(TARGET_COMMON_DIR)/
CPPFLAGS       += -I$(TARGET_SPEC_DIR)/

ifneq ($(OBJDIR),)
# Preferred over rules.mk's `%.o: %.c` for these paths: that one would look for
# a source inside OBJDIR, which does not exist, so this is the only rule whose
# prerequisite can be found. `-MD` puts the dependency file beside the object,
# so DEPS follows OBJDIR without further work.
$(OBJDIR)/%.o: %.c $(LIB_DIR)/lib$(LIBNAME).a
	$(Q)mkdir -p $(dir $(@))
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) $(ARCH_FLAGS) -o $(@) -c $(<)
endif

.PHONY         += debug size dfuse-flash dfu-flash reset
