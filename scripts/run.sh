#!/bin/bash
# scripts/run.sh - Minus 内核 QEMU 启动+调试脚本
# 用法：
#   普通运行：./scripts/run.sh
#   调试运行：DEBUG=1 ./scripts/run.sh

# ===================== 配置参数（可按需修改） =====================
# 调试开关：DEBUG=1 时启用 GDB 调试（暂停启动+监听 1235 端口）
if [ "$DEBUG" = "1" ]; then
    DEBUG_FLAGS="-gdb tcp::1235 -S"  # -S：启动后暂停，-gdb：监听 1235 端口
else
    DEBUG_FLAGS=""  # 非调试模式无额外参数
fi

# 输出目录（匹配 Kbuild 体系的 output/）
OUTDIR=`pwd`/output
# 内核镜像（Kbuild 编译产物）
KERNEL_ELF=${OUTDIR}/kernel.elf

# QEMU 核心参数（AArch64 virt 平台）
QEMU_CMD="qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a53 \
    -m 1G \
    -kernel ${KERNEL_ELF} \
    -nographic \
    ${DEBUG_FLAGS}"

# ===================== 执行启动 =====================
echo "\033[32m[Minus] Starting QEMU (AArch64 virt) ...\033[0m"
echo "\033[33m[Minus] Kernel: ${KERNEL_ELF}\033[0m"
if [ "$DEBUG" = "1" ]; then
    echo "\033[33m[Minus] Debug mode: GDB connect to tcp::1235\033[0m"
fi

# 执行 QEMU 命令
${QEMU_CMD}

# 退出清理
echo "\033[31m[Minus] QEMU exited\033[0m"
