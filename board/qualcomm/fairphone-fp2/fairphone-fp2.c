// SPDX-License-Identifier: GPL-2.0+
/*
 * Fairphone 2
 */

#include <dm.h>
#include <env.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <fdt_support.h>
#include <init.h>
#include <log.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/libfdt.h>
#include <power/pmic.h>
#include <vsprintf.h>

/* MMCC: power domain and bus clock of the display subsystem */
#define MMCC_MDSS_GDSCR		0xfd8c2304
#define GDSC_PWR_ON		BIT(31)
#define MMCC_MDSS_AHB_CBCR	0xfd8c2308
#define CBCR_CLK_OFF		BIT(31)

/* CTL0 of the MDP in MDSS 1.2 */
#define MDP5_CTL0_TOP		0xfd900614
#define CTL_TOP_CMD_MODE	BIT(17)

/*
 * LK only powers and clocks the display subsystem when it drives a panel,
 * e.g. not on a bare mainboard. Reading its registers otherwise hangs the
 * bus and resets the phone.
 */
static bool fp2_display_on(void)
{
	return (readl(MMCC_MDSS_GDSCR) & GDSC_PWR_ON) &&
	       !(readl(MMCC_MDSS_AHB_CBCR) & CBCR_CLK_OFF);
}

void qcom_board_init(void)
{
	struct udevice *dev;

	if (fp2_display_on())
		return;

	log_info("Display: not set up by LK, console on serial only\n");
	if (!uclass_find_first_device(UCLASS_VIDEO, &dev) && dev)
		device_unbind(dev);
}

/* PM8941 MISC: real time state of the USB ID line, high while floating */
#define PM8941_MISC_INT_RT_STS	0x910
#define USB_ID_RT_STS		BIT(0)

/* An OTG adapter grounds the USB ID line: the phone is the USB host */
static bool fp2_usb_id_grounded(void)
{
	struct udevice *pmic;
	int val;

	if (uclass_get_device_by_name(UCLASS_PMIC, "pm8941@0", &pmic))
		return false;

	val = pmic_reg_read(pmic, PM8941_MISC_INT_RT_STS);

	return val >= 0 && !(val & USB_ID_RT_STS);
}

/*
 * Variables describing this boot only, which a saved environment must not
 * carry over: without a display, the environment skips the key menu meant
 * for the screen; the USB role decides the boot order; the userdata block
 * map is created again.
 */
void qcom_late_init(void)
{
	env_set("headless", fp2_display_on() ? NULL : "1");
	env_set("usb_id_host", fp2_usb_id_grounded() ? "1" : NULL);
	env_set("ud_dev", NULL);
}

/*
 * The Fairphone 2 has either an OTM1902B command mode panel or an S6D6FA1
 * video mode one, both described by the generic "fairphone,fp2-panel" node
 * of the Linux device tree. Like lk2nd, give the OS the compatible of the
 * panel LK set up, which the mode of the display controller tells apart.
 */
static int fp2_fixup_panel(void *blob)
{
	const char *panel;
	u32 top;
	int node;

	node = fdt_node_offset_by_compatible(blob, -1, "fairphone,fp2-panel");
	if (node < 0 || !fp2_display_on())
		return 0;

	top = readl(MDP5_CTL0_TOP);
	if (!top)
		return 0;

	if (top & CTL_TOP_CMD_MODE)
		panel = "fairphone,fp2-panel-otm1902b";
	else
		panel = "fairphone,fp2-panel-s6d6fa1";

	log_info("Panel: %s\n", panel);

	return fdt_setprop_string(blob, node, "compatible", panel);
}

/*
 * The WCNSS has no address of its own: the bootloader gives the WLAN and
 * Bluetooth nodes one, without which Linux cannot bring wlan0 up. Like
 * lk2nd, derive stable, locally administered addresses from the serial
 * number (02:00:<serial> for WLAN, 02:01:<serial> for Bluetooth, whose
 * property is least significant byte first).
 */
static int fp2_fixup_wcnss(void *blob)
{
	const char *serial = env_get("serial#");
	u8 mac[6], bd[6];
	u32 sn;
	int node, ret, i;

	if (!serial)
		return 0;

	sn = hextoul(serial, NULL);
	mac[0] = 0x02;
	mac[1] = 0x00;
	mac[2] = sn >> 24;
	mac[3] = sn >> 16;
	mac[4] = sn >> 8;
	mac[5] = sn;

	node = fdt_node_offset_by_compatible(blob, -1, "qcom,wcnss-wlan");
	if (node >= 0) {
		ret = fdt_setprop(blob, node, "local-mac-address", mac, sizeof(mac));
		if (ret)
			return ret;
	}

	mac[1] = 0x01;
	for (i = 0; i < sizeof(bd); i++)
		bd[i] = mac[sizeof(mac) - 1 - i];

	node = fdt_node_offset_by_compatible(blob, -1, "qcom,wcnss-bt");
	if (node >= 0)
		return fdt_setprop(blob, node, "local-bd-address", bd, sizeof(bd));

	return 0;
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	int ret;

	ret = fp2_fixup_wcnss(blob);
	if (ret)
		return ret;

	return fp2_fixup_panel(blob);
}
