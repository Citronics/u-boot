// SPDX-License-Identifier: GPL-2.0+
/*
 * Debug UART that appends to a buffer in RAM, for boards without a usable
 * UART. Layout at CONFIG_DEBUG_UART_BASE: magic, character count, then the
 * characters.
 */

#include <debug_uart.h>
#include <linux/types.h>

#define RAMLOG_MAGIC	0x474f4c52	/* "RLOG" */
#define RAMLOG_SIZE	0x100000

struct ramlog {
	u32 magic;
	u32 len;
	u32 reserved[2];
	char buf[];
};

static inline void _debug_uart_init(void)
{
	struct ramlog *log = (struct ramlog *)CONFIG_DEBUG_UART_BASE;

	if (log->magic != RAMLOG_MAGIC || log->len >= RAMLOG_SIZE) {
		log->magic = RAMLOG_MAGIC;
		log->len = 0;
	}
}

static inline void _debug_uart_putc(int ch)
{
	struct ramlog *log = (struct ramlog *)CONFIG_DEBUG_UART_BASE;

	if (log->len < RAMLOG_SIZE - sizeof(*log))
		log->buf[log->len++] = ch;
}

DEBUG_UART_FUNCS
