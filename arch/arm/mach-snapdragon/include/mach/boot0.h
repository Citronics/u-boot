/* SPDX-License-Identifier: GPL-2.0+ */
#if defined(CONFIG_SNAPDRAGON_ARMV7)
/*
 * The vector table followed by a zImage header: LK looks for the appended
 * device tree at the offset stored in the zImage end address field.
 */
_start:
	ARM_VECTORS
	.word	0
	.word	0x016f2818				/* zImage magic */
	.word	0					/* position independent */
	.word	_image_binary_end - CONFIG_TEXT_BASE	/* image end */
#elif defined(CONFIG_SPL_BUILD)
#if defined(CONFIG_BOOT0_SDM845_WORKAROUND)
#include "sdm845_spl_boot0.h"
#else
	b	reset
#endif
#else
#if defined(CONFIG_BOOT0_MSM8916_PSCI_WORKAROUND)
#include "msm8916_boot0.h"
#else
	b	reset
#endif
#endif
