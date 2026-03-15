# Minus 顶层 Makefile（仅全局配置，不包含编译规则）
# 内核版本信息
MAKEFLAGS += -s
MINUS_VERSION := 0.1
MINUS_ARCH    := aarch64
MINUS_PLAT    := qemu-virt

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

# 编译标志（AArch64 裸机必备）
KBUILD_CFLAGS := -I ${TOPDIR}/include \
		 -D__MINUS__ \
                 -march=armv8-a \
                 -mgeneral-regs-only \
                 -ffreestanding \
                 -nostdlib \
                 -g \
                 -Wall \
                 -Werror  # 警告视为错误（强制规范代码）

# 链接标志（指定链接脚本和代码起始地址）
KBUILD_LDFLAGS := -m aarch64elf \
                  -T kernel/kernel.ld.s \
                  -Ttext 0x40200000

# ===================== 核心目标 =====================
# 默认目标：编译内核 + 生成二进制文件
all: prepare $(TARGET) $(BIN_TARGET)
	@echo "\033[32m[Minus] Compile success! 🚀\033[0m"
	@echo "\033[32m  ELF: $(TARGET)\033[0m"
	@echo "\033[32m  BIN: $(BIN_TARGET)\033[0m"

# 准备输出目录（自动创建，避免报错）
prepare:
	@echo "\033[33m[Minus] Preparing output dir: $(OUTPUT)\033[0m"
	@mkdir -p $(OUTPUT) $(OUTPUT)/.tmp

# 调用顶层 Kbuild 执行编译（核心：-f 指定规则文件为 Kbuild）
$(TARGET):
	@echo "\033[33m[Minus] Starting Kbuild compile...\033[0m"
	$(MAKE) -f Kbuild \
	        CC=$(CC) \
	        LD=$(LD) \
		OBJCOPY=${OBJCOPY} \
	        CFLAGS="$(KBUILD_CFLAGS)" \
	        LDFLAGS="$(KBUILD_LDFLAGS)" \
	        OUTPUT=$(OUTPUT) \
	        TOPDIR=$(TOPDIR)

# 转换 ELF 为二进制文件（QEMU 可直接加载）
$(BIN_TARGET): $(TARGET)
	@echo "\033[33m[Minus] Generating binary file...\033[0m"
	$(OBJCOPY) -O binary $< $@

# 清理所有产物
clean:
	@echo "\033[31m[Minus] Cleaning all output...\033[0m"
	$(RM) $(OUTPUT)
	@echo "\033[32m[Minus] Clean success! ✨\033[0m"

# 伪目标声明（避免与同名文件冲突）
.PHONY: all prepare clean
