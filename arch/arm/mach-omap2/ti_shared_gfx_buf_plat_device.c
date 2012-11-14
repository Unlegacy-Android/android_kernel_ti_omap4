/*
 * ION Initialization for OMAP5.
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/platform_device.h>
#include <video/ti_shared_gfx_buf.h>

static struct ti_gfx_buf_mgr_platform_data ti_gfx_buf_mgr_data = {
        .for_future_use = 0 /* For future use */
};

static struct platform_device ti_gfx_buf_mgr_device = {
	.name = "ti-gfx-buf-mgr-dev",
	.id = -1,
	.dev = {
		.platform_data = &ti_gfx_buf_mgr_data,
	},
};

void __init omap5_register_ti_gfx_buf_mgr(void)
{
        printk("%s", __func__);
	platform_device_register(&ti_gfx_buf_mgr_device);
}
