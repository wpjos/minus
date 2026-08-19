# Minus 顶层 Kbuild（仅声明，无实现）
# 声明要编译的子目录（仅告诉 Kbuild 体系：需要编译这些目录）
obj-y += lib/
obj-y += kernel/

# 声明最终目标（仅定义目标名，实现逻辑在 include 文件）
# .DEFAULT_GOAL 必须在 include 之前显式声明：Kbuild.include 对未声明者
# 默认钉到 built-in.o（.d 依赖文件的首目标会劫持 make 默认目标）。
.DEFAULT_GOAL := all
all: $(OUTPUT)/kernel.elf

# 引入实现逻辑（所有「怎么做」的代码都在这，Kbuild 仅声明）
include $(TOPDIR)/scripts/Kbuild.include
