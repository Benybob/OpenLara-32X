ROOTDIR ?= $(if $(GENDEV),$(GENDEV),/opt/toolchains/sega)
export ROOTDIR
.DEFAULT_GOAL := all

TARGET = OpenLara
BUILD = build
SOURCES = . asm ../../fixed
PYTHON ?= python3
SFX_OUTPUTS = sound_assets.h data/sfx-ima.bin data/sfx-manifest.json
SFX_SOURCES = $(wildcard ../gba/data/*.PKD) ../../../tools/32x/pack_sfx.py

$(SFX_OUTPUTS) &: $(SFX_SOURCES)
	$(PYTHON) ../../../tools/32x/pack_sfx.py

LDSCRIPTSDIR ?= $(CURDIR)/ldscripts

SHPREFIX = $(ROOTDIR)/sh-elf/bin/sh-elf-
SHCC = $(SHPREFIX)gcc
SHXX = $(SHPREFIX)g++
SHPP = $(SHPREFIX)cpp
SHAS = $(SHPREFIX)as
SHLD = $(SHPREFIX)ld
SHOBJC = $(SHPREFIX)objcopy

CC_VER := $(shell $(SHCC) -dumpfullversion -dumpversion)

MDPREFIX = $(ROOTDIR)/m68k-elf/bin/m68k-elf-
MDAS = $(MDPREFIX)as
MDLD = $(MDPREFIX)ld

LIBPATH = -L$(ROOTDIR)/sh-elf/lib -L$(ROOTDIR)/sh-elf/lib/gcc/sh-elf/$(CC_VER) -L$(ROOTDIR)/sh-elf/sh-elf/lib
INCPATH = -I. -I../../fixed -I$(ROOTDIR)/sh-elf/include -I$(ROOTDIR)/sh-elf/sh-elf/include

OPT ?= -O2
SHCCFLAGS = $(CPPFLAGS) -MMD -MP -D__32X__ -m2 -mb $(OPT) -Wall -Wno-class-memaccess -g -c -fno-rtti -fno-exceptions -fomit-frame-pointer -fshort-enums -frandom-seed=0
SHLDFLAGS = -T $(LDSCRIPTSDIR)/mars.ld -Wl,-Map=$(BUILD)/output.map -nostdlib
SHASFLAGS = --big

MDLDFLAGS = -T $(LDSCRIPTSDIR)/md.ld --oformat binary
MDASFLAGS = -m68000 --register-prefix-optional

DD = dd
RM = rm -f

FILES_AS = $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.s))
FILES_CC = $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.c))
FILES_XX = $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.cpp))

LIBS = $(LIBPATH) -lm -lc -lgcc -lgcc-Os-4-200 -lnosys
OBJS =  $(addprefix $(BUILD)/, $(notdir $(FILES_AS:%.s=%.o)))
OBJS += $(addprefix $(BUILD)/, $(notdir $(FILES_CC:%.c=%.o)))
OBJS += $(addprefix $(BUILD)/, $(notdir $(FILES_XX:%.cpp=%.o)))

.PHONY: all dump clean

all: $(TARGET).32x

$(OBJS): | $(BUILD)
$(BUILD)/crt0.o: src-md/m68k.bin
$(BUILD)/data.o: $(wildcard data/*.PKD) data/sfx-ima.bin
$(BUILD)/sound.o: sound_assets.h

-include $(OBJS:.o=.d)

dump:
	@[ -d dump ] || mkdir -p dump
	$(SHXX) $(SHCCFLAGS) $(INCPATH) -S -o dump/main.s main.cpp
	$(SHXX) $(SHCCFLAGS) $(INCPATH) -S -o dump/render.s render.cpp
	$(SHXX) $(SHCCFLAGS) $(INCPATH) -S -o dump/sound.s sound.cpp
	$(SHXX) $(SHCCFLAGS) $(INCPATH) -S -o dump/common.s ../../fixed/common.cpp

$(BUILD):
	@[ -d $@ ] || mkdir -p $@

$(TARGET).32x: $(TARGET).elf
	$(SHOBJC) -O binary $< temp.bin
	$(DD) if=temp.bin of=$@ bs=1024K conv=sync
	$(RM) -f temp.bin

$(TARGET).elf: $(OBJS) $(LDSCRIPTSDIR)/mars.ld
	$(SHCC) $(SHLDFLAGS) $(OBJS) $(LIBS) -o $(TARGET).elf

src-md/m68k.bin: $(wildcard src-md/*.c src-md/*.s) src-md/Makefile $(LDSCRIPTSDIR)/mars-md.ld
	$(MAKE) -C src-md

$(BUILD)/%.o: %.s
	$(SHAS) $(SHASFLAGS) $(INCPATH) -o $@ $<

$(BUILD)/%.o: asm/%.s
	$(SHPP) $(CPPFLAGS) $(INCPATH) -MMD -MP -MF $(@:.o=.d) -MT $@ -o $(BUILD)/$(notdir $<) $<
	$(SHAS) $(SHASFLAGS) $(INCPATH) -o $@ $(BUILD)/$(notdir $<)

$(BUILD)/%.o: %.c
	$(SHCC) $(SHCCFLAGS) $(INCPATH) -o $@ $<

$(BUILD)/%.o: %.cpp
	$(SHXX) $(SHCCFLAGS) $(INCPATH) -o $@ $<

$(BUILD)/%.o: ../../fixed/%.cpp
	$(SHXX) $(SHCCFLAGS) $(INCPATH) -o $@ $<

clean:
	make clean -C src-md
	$(RM) $(BUILD)/* $(TARGET).32x $(TARGET).elf
