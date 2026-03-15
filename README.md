# Minus - A Minimal ARM64 Operating System

Minus 是一个从零开始编写的迷你 ARM64 操作系统，当前已移植了 libfdt，可以解析 DTB 设备树，支持在 QEMU 上运行和 GDB 调试。

## Quick Start

### Prerequisites

- QEMU (with ARM64 support)
- ARM64 cross compiler (`aarch64-elf-gcc`)
- make
- GDB (optional, for debugging)

### Build and Run

Just two steps:

```bash
# Step 1: Build the kernel
make

# Step 2: Run in QEMU (normal mode)
DEBUG=0 sh script/run.sh
