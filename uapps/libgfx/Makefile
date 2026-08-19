CFLAGS  := -ffreestanding -nostdlib -fno-stack-protector -fno-builtin \
           -march=armv8-a -mgeneral-regs-only -O2 -g -Wall -Werror \
           -I$(TOPDIR)/include/base -I$(TOPDIR)/include/uapi -I.

# 中间目标统一放到 output/ 下，源码目录保持干净
OBJDIR  := $(OUTPUT)/obj/uapps/libgfx

OBJS    := $(addprefix $(OBJDIR)/, fb.o gfx.o font.o font_8x8.o surface.o)

all: $(OUTPUT)/libgfx.a

$(OUTPUT)/libgfx.a: $(OBJS)
	@mkdir -p $(OUTPUT)
	$(AR) rcs $@ $(OBJS)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(OUTPUT)/libgfx.a

.PHONY: all clean
