/*
 * arch/arm/mach-omap2/board-bn-hd.h
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_OMAP_BOARD_BN_HD_H
#define _MACH_OMAP_BOARD_BN_HD_H

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
#define HUMMINGBIRD_EVT0		0x0
#define HUMMINGBIRD_EVT0B		0x1
#define HUMMINGBIRD_EVT1		0x2
#define HUMMINGBIRD_EVT2		0x3
#define HUMMINGBIRD_DVT			0x4
#define HUMMINGBIRD_PVT			0x5

int hummingbird_touch_init(void);
int hummingbird_panel_init(void);
int hummingbird_button_init(void);
void hummingbird_android_display_setup(void);
#endif

#ifdef CONFIG_MACH_OMAP_OVATION
#define OVATION_EVT0			0x0
#define OVATION_EVT0B			0x1
#define OVATION_EVT0C			0x2
#define OVATION_EVT1A			0x3
#define OVATION_EVT1B			0x4
#define OVATION_EVT2			0x5
#define OVATION_DVT				0x6
#define OVATION_PVT				0x7

int ovation_touch_init(void);
int ovation_panel_init(void);
int ovation_button_init(void);
void ovation_android_display_setup(void);
#endif

void bn_emif_init(void);
void bn_power_init(void);
void bn_wilink_init(void);

#endif
