# Minus 顶层 Makefile（仅全局配置，不包含编译规则）
# 内核版本信息
MAKEFLAGS += -s
MINUS_VERSION := 0.1
MINUS_ARCH    := aarch64
PLAT          ?= virt

# 默认目标仍然是 all（后面 defconfig 等目标不能抢默认）
.DEFAULT_GOAL := all

# 交叉编译器配置（Linux 内核风格）
CROSS_COMPILE ?= aarch64-elf-
CC            := $(CROSS_COMPILE)gcc
LD            := $(CROSS_COMPILE)ld
OBJCOPY       := $(CROSS_COMPILE)objcopy
RM            := rm -rf

# 输出目录（所有产物集中存放）
TOPDIR        := $(CURDIR)
OUTPUT        := $(TOPDIR)/output
TARGET        := $(OUTPUT)/kernel.elf
BIN_TARGET    := $(OUTPUT)/kernel.bin
DTB_DIR       := $(OUTPUT)/dtb
ROOTFS_DIR    := $(OUTPUT)/rootfs
ROOTFS        := $(ROOTFS_DIR)/rootfs.ext4
DTB_SRCS      := $(wildcard $(TOPDIR)/arch/dts/*.dts)
DTBS          := $(patsubst $(TOPDIR)/arch/dts/%.dts,$(DTB_DIR)/%.dtb,$(DTB_SRCS))

# 公共头文件搜索路径（C/汇编/链接脚本预处理共用）
KBUILD_CPPFLAGS := $(addprefix -I,$(wildcard $(TOPDIR)/include/*)) \
                   -I $(TOPDIR)/lib/libfdt

# 编译标志（AArch64 裸机必备）
KBUILD_CFLAGS := $(KBUILD_CPPFLAGS) \
                 -D__MINUS__ \
                 -include $(TOPDIR)/include/generated/autoconf.h \
                 -march=armv8-a \
                 -mgeneral-regs-only \
                 -ffreestanding \
                 -nostdlib \
                 -g \
                 -Wall \
                 -Werror  # 警告视为错误（强制规范代码）

# 链接标志（指定预处理后生成的链接脚本）
KBUILD_LDFLAGS := -m aarch64elf \
                  -T $(OUTPUT)/kernel.ld

# Kconfig / autoconf 生成工具
KCONFIG_PY    := $(TOPDIR)/scripts/kconfig.py
DEFCONFIG     := $(TOPDIR)/arch/configs/$(PLAT)_defconfig
AUTOCONF_H    := $(TOPDIR)/include/generated/autoconf.h

# ===================== Kconfig 目标 =====================
defconfig:
	@echo "\033[33m[Minus] Generating .config for PLAT=$(PLAT)...\033[0m"
	python3 $(KCONFIG_PY) defconfig --defconfig=$(DEFCONFIG)

oldconfig syncconfig:
	python3 $(KCONFIG_PY) syncconfig

# 自动根据当前 PLAT 生成 autoconf.h（prepare 时调用）
$(AUTOCONF_H): $(DEFCONFIG) $(TOPDIR)/Kconfig $(TOPDIR)/arch/Kconfig $(KCONFIG_PY)
	@python3 $(KCONFIG_PY) defconfig --defconfig=$(DEFCONFIG)

# ===================== 核心目标 =====================
# 默认目标：编译内核 + 生成二进制文件 + 编译设备树 + 用户态程序 + rootfs
all: prepare $(TARGET) $(BIN_TARGET) dtbs uapps rootfs
	@echo "\033[32m[Minus] Compile success! 🚀\033[0m"
	@echo "\033[32m  ELF: $(TARGET)\033[0m"
	@echo "\033[32m  BIN: $(BIN_TARGET)\033[0m"
	@echo "\033[32m  DTB: $(DTB_DIR)/\033[0m"
	@echo "\033[32m  SHELL: $(OUTPUT)/shell.elf\033[0m"
	@echo "\033[32m  FB_TEST: $(OUTPUT)/fb_test.elf\033[0m"

# 引入用户态程序编译规则
include $(TOPDIR)/uapps/Makefile

# 准备输出目录（自动创建，避免报错），并同步生成 autoconf.h
prepare: $(AUTOCONF_H)
	@echo "\033[33m[Minus] Preparing output dir: $(OUTPUT)\033[0m"
	@mkdir -p $(OUTPUT) $(OUTPUT)/.tmp $(DTB_DIR) ${ROOTFS_DIR}

# 调用顶层 Kbuild 执行编译（核心：-f 指定规则文件为 Kbuild）
$(TARGET): | prepare
$(TARGET):
	@echo "\033[33m[Minus] Starting Kbuild compile...\033[0m"
	$(MAKE) -f Kbuild \
	        CC=$(CC) \
	        LD=$(LD) \
	        OBJCOPY=${OBJCOPY} \
	        CFLAGS="$(KBUILD_CFLAGS)" \
	        CPPFLAGS="$(KBUILD_CPPFLAGS)" \
	        LDFLAGS="$(KBUILD_LDFLAGS)" \
	        OUTPUT=$(OUTPUT) \
	        TOPDIR=$(TOPDIR)

# 转换 ELF 为二进制文件（QEMU 可直接加载）
$(BIN_TARGET): $(TARGET)
	@echo "\033[33m[Minus] Generating binary file...\033[0m"
	$(OBJCOPY) -O binary $< $@

# 生成/刷新 rootfs.ext4，并安装用户态程序到 /bin
rootfs: $(ROOTFS) uapps-install

$(ROOTFS):
	@echo "\033[33m[Minus] Creating rootfs.ext4...\033[0m"
	@mkdir -p $(ROOTFS_DIR)
	dd if=/dev/zero of=$@ bs=1M count=1
	mkfs.ext4 -F -O ^64bit,^has_journal $@

dtbs: $(DTBS)
	@echo "\033[32m[Minus] DTB files generated:\033[0m"
	@for dtb in $(DTBS); do echo "\033[32m  $$dtb\033[0m"; done

$(DTB_DIR)/%.dtb: $(TOPDIR)/arch/dts/%.dts
	@mkdir -p $(DTB_DIR)
	@echo "\033[33m[Minus] Generating DTB: $@\033[0m"
	dtc -I dts -O dtb -o $@ $<

# 清理所有产物
clean:
	@echo "\033[31m[Minus] Cleaning all output...\033[0m"
	$(RM) $(OUTPUT)
	$(RM) $(TOPDIR)/.config $(TOPDIR)/include/generated
	$(RM) $(TOPDIR)/scripts/__pycache__
	$(MAKE) -C $(TOPDIR)/uapps TOPDIR=$(TOPDIR) OUTPUT=$(OUTPUT) uapps-clean
	@echo "\033[32m[Minus] Clean success! ✨\033[0m"

# 伪目标声明（避免与同名文件冲突）
.PHONY: all prepare clean dtbs defconfig oldconfig syncconfig rootfs
