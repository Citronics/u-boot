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
int ft_board_setup(void *blob, struct bd_info *bd)
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
