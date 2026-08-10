CC      := aarch64-elf-gcc
AR      := aarch64-elf-ar
CFLAGS  := -ffreestanding -nostdlib -fno-stack-protector -fno-builtin \
           -march=armv8-a -mgeneral-regs-only -O2 -g -Wall -Werror \
           -I$(TOPDIR)/include/base -I$(TOPDIR)/include/uapi -I.

OBJS    := fb.o gfx.o font.o font_8x8.o surface.o

all: $(OUTPUT)/libgfx.a

$(OUTPUT)/libgfx.a: $(OBJS)
	@mkdir -p $(OUTPUT)
	$(AR) rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(OUTPUT)/libgfx.a

.PHONY: all clean
