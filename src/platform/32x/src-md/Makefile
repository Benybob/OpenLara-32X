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

CCFLAGS = $(CPPFLAGS) -MMD -MP -m68000 -Wall -O2 -c -fomit-frame-pointer -ffreestanding -fno-builtin
ASFLAGS = -m68000 --register-prefix-optional
# Worker instructions are copied into writable 68000 RAM at boot.
LINKFLAGS = -T $(LDSCRIPTSDIR)/mars-md.ld -Wl,-Map=output.map,--no-warn-rwx-segments -nostdlib

DD = dd
RM = rm -f

TARGET = m68k
LIBS = $(LIBPATH) -lc -lgcc -lnosys
OBJS = crt0.o main.o kos.o cd.o worker.o worker_blob.o worker_fast.o

all: $(TARGET).bin

worker_blob.o: cd-worker.bin
worker.o cd-worker.o: worker_tables.h worker_decode.h ../worker.h
worker_fast.o: worker_fast_tables.inc

cd-worker.elf: cd-worker-start.o cd-worker.o worker_fast.o cd-worker.ld
	$(CC) -m68000 -nostdlib -Wl,-T,cd-worker.ld,--no-warn-rwx-segments cd-worker-start.o cd-worker.o worker_fast.o -lgcc -o $@

cd-worker.bin: cd-worker.elf
	$(OBJC) -O binary $< $@

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
