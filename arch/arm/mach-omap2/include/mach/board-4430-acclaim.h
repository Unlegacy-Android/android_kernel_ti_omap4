/*
 * linux/arch/arm/mach-omap2/board-4430-acclaim.h
 *
 * Copyright (C) 2011 Barnes & Noble.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _BOARD_4430_ACCLAIM_H
#define _BOARD_4430_ACCLAIM_H

#include <linux/regulator/consumer.h>

//#ifdef CONFIG_MACH_OMAP4_ACCLAIM
#define EVT0  0
#define EVT1A 1
#define EVT1B 2
#define EVT2  3
#define DVT   4

#include <asm/system.h>

static inline int acclaim_board_type(void)
{
	return system_rev;  // This is set by U-Boot
}

#define BN_USB_VENDOR_ID            0x2080
#define BN_USB_PRODUCT_ID_ACCLAIM   0x0004
#define BN_USB_MANUFACTURER_NAME	"Barnes & Noble"
#define BN_USB_PRODUCT_NAME			"Acclaim" // Replace when actual product name is known

#define ACCLAIM_RAM_CONSOLE_SIZE         (1 << CONFIG_LOG_BUF_SHIFT)
#define ACCLAIM_RAM_CONSOLE_START  (ACCLAIM_RAM_CONSOLE_END - ACCLAIM_RAM_CONSOLE_SIZE + 1)
#define ACCLAIM_RAM_CONSOLE_END    (0xA2000000 - 1)


struct boxer_panel_data {
	struct regulator *vlcd;
};


//#endif
#endif
