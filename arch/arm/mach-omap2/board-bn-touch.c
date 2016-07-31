/*
 * arch/arm/mach-omap2/board-bn-touch.c
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

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <plat/i2c.h>

#include <asm/mach-types.h>

#include "board-bn-hd.h"
#include "mux.h"

#if defined(CONFIG_TOUCHSCREEN_FT5X06) || defined(CONFIG_TOUCHSCREEN_FT5X06_MODULE)
#include <linux/input/ft5x06_ts.h>
#endif

#define TOUCHPANEL_GPIO_IRQ     37
#define TOUCHPANEL_GPIO_RESET   39

int Vdd_LCD_CT_PEN_request_supply(struct device *dev, const char *supply_name);
int Vdd_LCD_CT_PEN_enable(struct device *dev, const char *supply_name);
int Vdd_LCD_CT_PEN_disable(struct device *dev, const char *supply_name);
int Vdd_LCD_CT_PEN_release_supply(struct device *dev, const char *supply_name);

#if 0
static struct gpio bn_touch_gpios[] = {
	{ TOUCHPANEL_GPIO_IRQ,		GPIOF_IN,			"touch_irq"   },
	{ TOUCHPANEL_GPIO_RESET,	GPIOF_OUT_INIT_LOW,	"touch_reset" },
};

static int bn_touch_request_resources(struct device  *dev)
{
	// Request GPIO lines
	if (gpio_request_array(bn_touch_gpios, ARRAY_SIZE(bn_touch_gpios))) {
		dev_err(dev, "%s: Could not get touch gpios\n", __func__);
		goto err_gpio_request;
	}
	gpio_set_value(TOUCHPANEL_GPIO_RESET, 0);

	// Request the required power supplies
	if (Vdd_LCD_CT_PEN_request_supply(NULL, "vtp")) {
		dev_err(dev, "%s: Could not get touch supplies\n", __func__);
		goto err_regulator_get;
	}

	return 0;

err_regulator_get:
	gpio_free_array(bn_touch_gpios, ARRAY_SIZE(bn_touch_gpios));

err_gpio_request:
	return -EBUSY;
}

static int bn_touch_release_resources(struct device  *dev)
{
	gpio_free_array(bn_touch_gpios, ARRAY_SIZE(bn_touch_gpios));

	// Release the touch power supplies
	if (Vdd_LCD_CT_PEN_release_supply(NULL, "vtp"))
		dev_err(dev, "%s: Could not release touch supplies\n", __func__);

	return 0;
}
#endif

static int bn_touch_power_on(struct device  *dev)
{
	gpio_set_value(TOUCHPANEL_GPIO_RESET, 0);

	if (Vdd_LCD_CT_PEN_enable(NULL, "vtp")) {
		dev_err(dev, "%s: Could not enable touch supplies\n", __func__);
		return -EBUSY;
	}

	msleep(10);

	// Pull the nRESET line high after the power stabilises
	gpio_set_value(TOUCHPANEL_GPIO_RESET, 1);

	msleep(220);

	return 0;
}

static int bn_touch_power_off(struct device  *dev)
{
	gpio_set_value(TOUCHPANEL_GPIO_RESET, 0);
	msleep(2);

	if (Vdd_LCD_CT_PEN_disable(NULL, "vtp"))
		dev_err(dev, "%s: Could not disable touch supplies\n", __func__);

	return 0;
}

static int touch_power_init(bool enable)
{
	if (enable)
		if (Vdd_LCD_CT_PEN_request_supply(NULL, "vtp"))
			pr_err("ft5x06: %s: Could not get touch supplies\n", __func__);
	else
		if (Vdd_LCD_CT_PEN_release_supply(NULL, "vtp"))
			pr_err("ft5x06: %s: Could not release touch supplies\n", __func__);

	return 0;
}

static int touch_power_on(bool enable)
{
	if (enable)
		bn_touch_power_on(NULL);
	else
		bn_touch_power_off(NULL);

	return 0;
}

#if defined(CONFIG_TOUCHSCREEN_FT5X06) || defined(CONFIG_TOUCHSCREEN_FT5X06_MODULE)
static struct ft5x06_ts_platform_data ft5x06_platform_data = {
	.irqflags			= IRQF_TRIGGER_FALLING,
	.irq_gpio			= TOUCHPANEL_GPIO_IRQ,
	.irq_gpio_flags		= GPIOF_IN,
	.reset_gpio			= TOUCHPANEL_GPIO_RESET,
	.reset_gpio_flags	= GPIOF_OUT_INIT_LOW,
	.x_max				= machine_is_omap_ovation() ? 1920 : 900,
	.y_max				= machine_is_omap_ovation() ? 1280 : 1440,
	.flags				= machine_is_omap_ovation() ?
						  REVERSE_X_FLAG | REVERSE_Y_FLAG : 0,
	.ignore_id_check	= true,
	.power_init			= touch_power_init,
	.power_on			= touch_power_on,
#if 0 // AM: old pdata left here mostly for reference
	.max_tx_lines		= machine_is_omap_ovation() ? 38 : 32,
	.max_rx_lines		= machine_is_omap_ovation() ? 26 : 20,
	.maxx				= machine_is_omap_ovation() ? 1280 : 900,
	.maxy				= machine_is_omap_ovation() ? 1280 : 1440,
	.use_st				= FT_USE_ST,
	.use_mt				= FT_USE_MT,
	.use_trk_id			= FT_USE_TRACKING_ID,
	.use_sleep			= FT_USE_SLEEP,
	.use_gestures		= 1,
	.request_resources	= bn_touch_request_resources,
	.release_resources	= bn_touch_release_resources,
	.power_on			= bn_touch_power_on,
	.power_off			= bn_touch_power_off,
#endif
};
#endif

static struct i2c_board_info __initdata bn_i2c_3_boardinfo[] = {
#if defined(CONFIG_TOUCHSCREEN_FT5X06) || defined(CONFIG_TOUCHSCREEN_FT5X06_MODULE)
	{
		I2C_BOARD_INFO("ft5x06_ts", 0x70 >> 1),
		.platform_data = &ft5x06_platform_data,
		.irq = OMAP_GPIO_IRQ(TOUCHPANEL_GPIO_IRQ),
	},
#endif
};

int __init bn_touch_init(void)
{
	printk(KERN_INFO "%s: Registering touch controller device\n", __func__);

	i2c_register_board_info(3, bn_i2c_3_boardinfo, ARRAY_SIZE(bn_i2c_3_boardinfo));

	return 0;
}
