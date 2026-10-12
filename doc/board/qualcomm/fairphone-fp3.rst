.. SPDX-License-Identifier: GPL-2.0+
.. sectionauthor:: Marc Lainez <marc.lainez@gmail.com>

Fairphone 3 / 3+
================

The Fairphone 3 and 3+ are smartphones based on the Qualcomm SDM632
(MSM8953 family) SoC, with 4 GiB of RAM and 64 GiB of eMMC. Both models share
one device tree.

U-Boot runs as a second stage bootloader: the stock LK bootloader loads it from
the ``boot`` partition as an Android boot image, like a Linux kernel. It takes
the place lk2nd usually has on this phone.

Building
--------

::

  $ export CROSS_COMPILE=<aarch64 toolchain prefix>
  $ make O=.output qcom_defconfig qcom-phone.config fairphone-fp3.config
  $ make O=.output -j$(nproc)

The stock LK expects a gzipped kernel with the device tree appended, in an
Android boot image with 2 KiB pages and a base address of ``0x80000000``::

  $ gzip -9nc .output/u-boot-nodtb.bin > .output/u-boot-nodtb.bin.gz
  $ cat .output/u-boot-nodtb.bin.gz \
        .output/dts/upstream/src/arm64/qcom/sdm632-fairphone-fp3.dtb \
        > .output/u-boot-nodtb.bin.gz-dtb
  $ printf '\0' > .output/ramdisk.img
  $ mkbootimg --kernel .output/u-boot-nodtb.bin.gz-dtb \
        --ramdisk .output/ramdisk.img --base 0x80000000 --pagesize 2048 \
        --output u-boot-fp3.img

The device tree carries the ``qcom,msm-id``, ``qcom,board-id`` and
``qcom,pmic-id`` values the stock LK uses to select it.

Installation
------------

The bootloader has to be unlocked. Enter LK fastboot by holding Volume Down
while powering on. Try U-Boot from RAM first::

  $ fastboot boot u-boot-fp3.img

Then install it to the boot partition of the active slot::

  $ fastboot flash boot u-boot-fp3.img

The stock LK also applies the overlays from the ``dtbo`` partition to the
device tree it boots. Overlays meant for Android do not match the mainline
device tree, so ``dtbo`` should hold an empty overlay, as for lk2nd.

Usage
-----

The console is on the display, cleared when U-Boot starts, and on the debug
UART pads. In menus, Volume Up and Volume Down move
and Power selects.

Without a key pressed, U-Boot boots the first operating system it finds:

* any standard boot method (extlinux, EFI, ...) on the eMMC partitions,
* an image with its own partition table written to the ``userdata``
  partition, booted with the ``extlinux.conf`` of its first partition, as
  lk2nd does.

If nothing can be booted, the boot menu opens.

During the first five seconds of U-Boot, after the boot splash is shown:

* press Volume Up for the boot menu,
* press Volume Down for U-Boot fastboot.

The boot menu offers:

* Boot,
* USB disk (userdata): a USB mass storage device that exposes only the
  ``userdata`` partition, never the whole eMMC with the bootloaders,
* USB fastboot,
* USB serial console (``minicom -D /dev/ttyACM0`` on the host),
* Reboot to LK fastboot, Reboot and Power off.

Any key, Power included, leaves the USB disk and fastboot modes and returns
to the menu. Ejecting the disk on the host also ends the USB disk mode.

From the USB serial console, ``run ums_userdata`` and ``run fastboot_usb``
switch USB to the USB disk and fastboot modes, and ``run lk_fastboot``
reboots into the fastboot mode of the stock LK.

Holding a key while powering on is handled by LK instead: Volume Down enters
LK fastboot and Volume Up boots the recovery partition.
