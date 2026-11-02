ROOTDIR ?= $(if $(GENDEV),$(GENDEV),/opt/toolchains/sega)

PREFIX = $(ROOTDIR)/m68k-elf/bin/m68k-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJC = $(PREFIX)objcopy

CC_VER := $(shell $(CC) -dumpfullversion -dumpversion)

LDSCRIPTSDIR ?= $(CURDIR)/../ldscripts

LIBPATH = -L$(ROOTDIR)/m68k-elf/lib -L$(ROOTDIR)/m68k-elf/lib/gcc/m68k-elf/$(CC_VER) -L$(ROOTDIR)/m68k-elf/m68k-elf/lib
INCPATH = -I. -I$(ROOTDIR)/m68k-elf/include -I$(ROOTDIR)/m68k-elf/m68k-elf/include

CCFLAGS = -MMD -MP -m68000 -Wall -O1 -c -fomit-frame-pointer
ASFLAGS = -m68000 --register-prefix-optional
LINKFLAGS = -T $(LDSCRIPTSDIR)/mars-md.ld -Wl,-Map=output.map -nostdlib

DD = dd
RM = rm -f

TARGET = m68k
LIBS = $(LIBPATH) -lc -lgcc -lnosys
OBJS = crt0.o main.o kos.o cd.o

all: $(TARGET).bin

-include $(OBJS:.o=.d)

$(TARGET).bin: $(TARGET).elf
	$(OBJC) -O binary $< $(TARGET).bin

$(TARGET).elf: $(OBJS) $(LDSCRIPTSDIR)/mars-md.ld
	$(CC) $(LINKFLAGS) $(OBJS) $(LIBS) -o $(TARGET).elf

%.o: %.c
	$(CC) $(CCFLAGS) $(INCPATH) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $(INCPATH) $< -o $@

clean:
	$(RM) *.o *.d *.bin *.elf output.map
