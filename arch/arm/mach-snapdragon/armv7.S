/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * LK enters 32-bit kernels with r0 = 0, r1 = the machine type and r2 = the
 * device tree. Keep r2 for board_fdt_blob_setup().
 *
 * LK also leaves branch prediction enabled while the MMU is off. U-Boot
 * keeps the MMU off, and on Krait predicted fetches then reach addresses the
 * secure world blocks: the SoC hangs until the secure watchdog resets it,
 * at points that move with the code layout. Turn branch prediction off and
 * drop what LK left in the predictor.
 */

#include <linux/linkage.h>
#include <asm/system.h>

ENTRY(save_boot_params)
	mrc	p15, 0, r3, c1, c0, 0		@ SCTLR
	bic	r3, r3, #CR_Z
	mcr	p15, 0, r3, c1, c0, 0
	mov	r3, #0
	mcr	p15, 0, r3, c7, c5, 6		@ BPIALL
	dsb
	isb
	ldr	r3, =qcom_prev_bl_fdt
	str	r2, [r3]
	b	save_boot_params_ret
ENDPROC(save_boot_params)

	.section .data
	.align	2
	.globl	qcom_prev_bl_fdt
qcom_prev_bl_fdt:
	.word	0
