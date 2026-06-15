qemu-system-aarch64 -machine virt,gic-version=3,dumpdtb=cortex-a72-virt.dtb -cpu cortex-a72 -smp 4 -m 1G -nographic
dtc -I dtb -O dts -o cortex-a72-virt.dts cortex-a72-virt.dtb
