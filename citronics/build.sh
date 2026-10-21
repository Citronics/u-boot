#!/bin/sh
#
# Build U-Boot as the Android boot images the stock LK of each phone loads
# from its boot partition.
#
# usage: citronics/build.sh [fp2|fp3]... [-o OUTDIR]
#
# With no phone given, both are built. Outputs, in OUTDIR (default:
# build-citronics, which .gitignore covers):
#   u-boot-fp2.img - Fairphone 2 (MSM8974, 32-bit): u-boot.bin with its
#                    device tree appended
#   u-boot-fp3.img - Fairphone 3 / 3+ (SDM632, 64-bit): gzipped u-boot-nodtb.bin
#                    with u-boot.dtb appended
# Flash with `fastboot flash boot u-boot-<phone>.img`, or try it first with
# `fastboot boot u-boot-<phone>.img`.

set -e

SRC=$(cd "$(dirname "$0")/.." && pwd)
OUT=build-citronics
PHONES=

while [ $# -gt 0 ]; do
	case "$1" in
	-o) OUT=$2; shift 2 ;;
	fp2|fp3) PHONES="$PHONES $1"; shift ;;
	*) echo "usage: $0 [fp2|fp3]... [-o OUTDIR]" >&2; exit 1 ;;
	esac
done
PHONES=${PHONES:-fp2 fp3}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
JOBS=$(nproc)

for phone in $PHONES; do
	build="$OUT/build-$phone"
	case $phone in
	fp2)
		make -C "$SRC" O="$build" CROSS_COMPILE=arm-linux-gnueabihf- \
			fairphone-fp2_defconfig
		make -C "$SRC" O="$build" CROSS_COMPILE=arm-linux-gnueabihf- \
			-j"$JOBS"
		# LK loads the image at 0x8000 and copies the device tree
		# appended to u-boot.bin to 0x01e00000.
		python3 "$SRC/citronics/mkbootimg.py" \
			--kernel "$build/u-boot.bin" \
			--output "$OUT/u-boot-fp2.img"
		;;
	fp3)
		make -C "$SRC" O="$build" CROSS_COMPILE=aarch64-linux-gnu- \
			qcom_defconfig qcom-phone.config fairphone-fp3.config
		make -C "$SRC" O="$build" CROSS_COMPILE=aarch64-linux-gnu- \
			-j"$JOBS"
		gzip -9nc "$build/u-boot-nodtb.bin" > "$build/kernel"
		cat "$build/u-boot.dtb" >> "$build/kernel"
		python3 "$SRC/citronics/mkbootimg.py" \
			--kernel "$build/kernel" \
			--base 0x80000000 \
			--ramdisk-offset 0x01000000 \
			--tags-offset 0x100 \
			--output "$OUT/u-boot-fp3.img"
		;;
	esac
	echo "$OUT/u-boot-$phone.img"
done
