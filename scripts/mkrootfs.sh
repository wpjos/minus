#!/bin/bash
set -e

SIZE_MB=${1:-64}
OUT_IMG=${ROOTFS}

mkdir -p "$(dirname "$OUT_IMG")"
rm -f "$OUT_IMG"

dd if=/dev/zero of="$OUT_IMG" bs=1M count=$SIZE_MB status=none
mkfs.ext4 -F -O ^64bit,^has_journal "$OUT_IMG"

# Install user apps if built
if [ -f "$TOPDIR/output/shell.elf" ]; then
	debugfs -w "$OUT_IMG" <<EOF
mkdir /bin
write $TOPDIR/output/shell.elf /bin/shell
sif /bin/shell mode 0100755
EOF
fi

if [ -f "$TOPDIR/output/fb_test.elf" ]; then
	debugfs -w "$OUT_IMG" <<EOF
write $TOPDIR/output/fb_test.elf /bin/fb_test
sif /bin/fb_test mode 0100755
EOF
fi

echo "Created $OUT_IMG (${SIZE_MB} MB)"
