.. SPDX-License-Identifier: GPL-2.0+
.. sectionauthor:: Marc Lainez <marc.lainez@gmail.com>

Fairphone 2
===========

The Fairphone 2 is a smartphone based on the 32-bit Qualcomm MSM8974AB
(Snapdragon 801) SoC, with 2 GiB of RAM and 32 GiB of eMMC.

U-Boot runs as a second stage bootloader: the stock LK bootloader (or lk2nd)
loads it from the ``boot`` partition as an Android boot image, like a 32-bit
Linux kernel. It takes the place lk2nd usually has on this phone.

U-Boot reuses what LK leaves running: the display, the eMMC and the PMICs.
It drives the eMMC, the PM8941 keys, the display console (through the MDP
pipe LK scans out, including command mode panels) and USB in device mode.

Building
--------

::

  $ export CROSS_COMPILE=<armv7 toolchain prefix>
  $ make O=.output fairphone-fp2_defconfig
  $ make O=.output -j$(nproc)

``u-boot.bin`` starts with a zImage header whose end address field points
right after the code, where the device tree is appended. LK finds the device
tree there, as it does for a zImage with an appended device tree. Pack it
into an Android boot image loaded at ``0x8000``, with the device tree copied
to ``0x01e00000``::

  $ printf '\0' > .output/ramdisk.img
  $ mkbootimg --kernel .output/u-boot.bin --ramdisk .output/ramdisk.img \
        --base 0x00000000 --kernel_offset 0x00008000 \
        --ramdisk_offset 0x02000000 --tags_offset 0x01e00000 \
        --pagesize 2048 --output u-boot-fp2.img

The device tree carries the ``qcom,msm-id`` and ``qcom,board-id`` values the
stock LK uses to select it.

Most units have no reachable UART. ``board/qualcomm/debug-msm8974.config``
keeps the console in a RAM buffer instead, which the previous bootloader can
read back after a warm reset (``fastboot oem debug readl`` in lk2nd).

Installation
------------

The bootloader has to be unlocked. Try U-Boot from RAM first, from the
fastboot mode of LK or lk2nd::

  $ fastboot boot u-boot-fp2.img

Then install it to the boot partition::

  $ fastboot flash boot u-boot-fp2.img

Usage
-----

The console is on the display, cleared when U-Boot starts, and on the debug
UART pads (BLSP1 UART2, 115200 8N1, set up by LK). Pressing a key on the UART
during the three second boot delay stops autoboot at the U-Boot prompt. In
menus, Volume Up and Volume Down move and Power selects.

USB host mode works with a powered hub (the phone does not supply VBUS): a
USB Ethernet dongle (ASIX AX88772/AX88179, Realtek RTL8152/8153, SMSC95xx,
MCS7830) is found by ``usb start``, then ``dhcp`` and ``tftpboot`` can be
used, e.g. ``tftpboot ${kernel_addr_r} zImage``.

``saveenv`` stores the environment, in two copies, at the end of the
``boot`` partition, after the U-Boot image. Flashing U-Boot keeps it,
erasing ``boot`` clears it. A saved environment replaces the built-in one,
also after an update of U-Boot: ``env default -a; saveenv`` goes back to the
built-in one.

On a board without a panel LK leaves the display subsystem off: U-Boot then
leaves it alone, sets ``headless=1`` and skips the screen and key menu, only
the boot delay remains on the UART.

Without a key pressed, U-Boot boots the first operating system it finds:

* standard boot (a ``boot.scr``, else an ``extlinux.conf``) on the microSD
  card, then on the first partition of the eMMC, the only one it scans as
  none is marked bootable. When the phone is the USB host, standard boot
  first tries USB storage, then the network (a ``boot.scr`` over TFTP, or a
  PXE ``extlinux`` configuration). The USB ID line, grounded by an OTG
  adapter, gives the role; ``usb_mode=host`` or ``usb_mode=device`` forces
  it, e.g. on a carrier board that does not ground ID,
* an image with its own partition table written to the ``userdata``
  partition, booted with the ``boot.scr`` or the ``extlinux.conf`` of its
  first partition. A ``boot.scr`` can choose between A/B slots of that
  image, for instance for RAUC,
* a bare filesystem written to the ``userdata`` partition, booted with its
  ``/boot/boot.scr`` (with the variables standard boot sets: ``devtype``,
  ``devnum``, ``distro_bootpart``, ``prefix``) or, as lk2nd does, its
  ``extlinux.conf``.

If nothing can be booted, the boot menu opens.

During the first five seconds of U-Boot:

* press Volume Up for the boot menu,
* press Volume Down for U-Boot fastboot.

The boot menu offers:

* Boot,
* USB disk (userdata): a USB mass storage device that exposes only the
  ``userdata`` partition, never the whole eMMC with the bootloaders,
* USB fastboot,
* USB serial console (``minicom -D /dev/ttyACM0`` on the host),
* Reboot to bootloader fastboot (LK or lk2nd), Reboot and Power off.

Any key, Power included, leaves the USB disk and fastboot modes and returns
to the menu. Ejecting the disk on the host also ends the USB disk mode.

From the USB serial console, ``run ums_userdata`` and ``run fastboot_usb``
switch USB to the USB disk and fastboot modes, and ``run lk_fastboot``
reboots into the fastboot mode of LK.
