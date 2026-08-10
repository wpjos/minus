# Userspace applications build rules
# Included by the top-level Makefile; expects TOPDIR and OUTPUT to be set.

UAPPS_DIR     := $(TOPDIR)/uapps

SHELL_DIR     := $(UAPPS_DIR)/shell
LIBGFX_DIR    := $(UAPPS_DIR)/libgfx
FB_TEST_DIR   := $(UAPPS_DIR)/fb_test

SHELL_ELF     := $(OUTPUT)/shell.elf
LIBGFX_A      := $(OUTPUT)/libgfx.a
FB_TEST_ELF   := $(OUTPUT)/fb_test.elf

UAPP_CC       ?= $(CROSS_COMPILE)gcc
UAPP_CFLAGS   := -ffreestanding -nostdlib -fno-stack-protector -fno-builtin \
                 -march=armv8-a -mgeneral-regs-only -O2 -g -Wall -Werror \
                 -I$(TOPDIR)/include/base -I$(TOPDIR)/include/uapi

.PHONY: uapps uapps-install uapps-clean

uapps: | prepare $(SHELL_ELF) $(FB_TEST_ELF)

$(LIBGFX_A): $(wildcard $(LIBGFX_DIR)/*.c) $(LIBGFX_DIR)/Makefile
	@echo "\033[33m[Minus] Building libgfx...\033[0m"
	@mkdir -p $(OUTPUT)
	$(MAKE) -C $(LIBGFX_DIR) TOPDIR=$(TOPDIR) OUTPUT=$(OUTPUT)

$(SHELL_ELF): $(SHELL_DIR)/shell.c $(SHELL_DIR)/shell.lds
	@echo "\033[33m[Minus] Building user shell...\033[0m"
	@mkdir -p $(OUTPUT)
	$(UAPP_CC) $(UAPP_CFLAGS) \
		-T $(SHELL_DIR)/shell.lds -o $@ $(SHELL_DIR)/shell.c

$(FB_TEST_ELF): $(FB_TEST_DIR)/fb_test.c $(LIBGFX_A) $(FB_TEST_DIR)/fb_test.lds
	@echo "\033[33m[Minus] Building fb_test...\033[0m"
	@mkdir -p $(OUTPUT)
	$(UAPP_CC) $(UAPP_CFLAGS) -I$(LIBGFX_DIR) \
		-T $(FB_TEST_DIR)/fb_test.lds -o $@ $(FB_TEST_DIR)/fb_test.c \
		$(LIBGFX_A)

uapps-install: $(ROOTFS) $(SHELL_ELF) $(FB_TEST_ELF)
	@echo "\033[33m[Minus] Installing apps into rootfs...\033[0m"
	@printf '%s\n' \
		"mkdir /bin" \
		"rm /bin/shell" \
		"rm /bin/fb_test" \
		"write $(SHELL_ELF) /bin/shell" \
		"write $(FB_TEST_ELF) /bin/fb_test" \
		"chmod 0755 /bin/shell" \
		"chmod 0755 /bin/fb_test" | \
		debugfs -w $(ROOTFS) >/dev/null 2>&1 || true

uapps-clean:
	$(MAKE) -C $(LIBGFX_DIR) TOPDIR=$(TOPDIR) OUTPUT=$(OUTPUT) clean
	rm -f $(SHELL_ELF) $(FB_TEST_ELF)
