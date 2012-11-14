/*
 * arch/arm/mach-omap2/omap5_ion.h
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

#ifndef _OMAP5_TI_SHARED_GFX_BUF_H
#define _OMAP5_TI_SHARED_GFX_BUF_H

#ifdef CONFIG_TI_SHARED_GFX_BUF
void omap5_register_ti_gfx_buf_mgr(void);
#else
static inline void omap5_register_ti_gfx_buf_mgr(void) { return; }
#endif

#endif // _OMAP5_TI_SHARED_GFX_BUF_H
