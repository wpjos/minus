# Minus 顶层 Kbuild（仅声明，无实现）
# 声明要编译的子目录（仅告诉 Kbuild 体系：需要编译这些目录）
obj-y += arch/
obj-y += lib/
obj-y += driver/
obj-y += kernel/
obj-y += dts/

# 声明最终目标（仅定义目标名，实现逻辑在 include 文件）
all: $(OUTPUT)/kernel.elf

# 引入实现逻辑（所有「怎么做」的代码都在这，Kbuild 仅声明）
include $(TOPDIR)/scripts/Kbuild.include
