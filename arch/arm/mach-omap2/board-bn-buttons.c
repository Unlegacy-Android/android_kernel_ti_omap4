/*
 * Button and Button LED support for Nook HDs.
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/leds.h>
#include <linux/leds_pwm.h>

#include <plat/omap4-keypad.h>

#include "board-bn-hd.h"
#include "mux.h"

static int bn_keymap[] = {
	KEY(0, 0, KEY_VOLUMEUP),
	KEY(1, 0, KEY_VOLUMEDOWN),
};

static struct omap_device_pad keypad_pads[] = {
	{	.name   = "kpd_col0.kpd_col0",
		.enable = OMAP_WAKEUP_EN | OMAP_MUX_MODE0,
	},
	{	.name   = "kpd_row0.kpd_row0",
		.enable = OMAP_PULL_ENA | OMAP_PULL_UP |
				OMAP_WAKEUP_EN | OMAP_MUX_MODE0 |
				OMAP_INPUT_EN,
	},
	{	.name   = "kpd_row1.kpd_row1",
		.enable = OMAP_PULL_ENA | OMAP_PULL_UP |
				OMAP_WAKEUP_EN | OMAP_MUX_MODE0 |
				OMAP_INPUT_EN,
	},
};

static struct matrix_keymap_data bn_keymap_data = {
	.keymap			= bn_keymap,
	.keymap_size	= ARRAY_SIZE(bn_keymap),
};

static struct omap4_keypad_platform_data bn_keypad_data = {
	.keymap_data	= &bn_keymap_data,
	.rows			= 2,
	.cols			= 1,
};

static struct omap_board_data keypad_data = {
	.id				= 1,
	.pads			= keypad_pads,
	.pads_cnt		= ARRAY_SIZE(keypad_pads),
};

static struct gpio_led bn_gpio_leds[] = {
	{
		.name		= "omap4:green:debug0",
		.gpio		= 82,
	},

};

static struct gpio_led_platform_data bn_led_data = {
	.leds			= bn_gpio_leds,
	.num_leds		= ARRAY_SIZE(bn_gpio_leds),
};

static struct platform_device bn_leds_gpio = {
	.name		= "leds-gpio",
	.id			= -1,
	.dev = {
		.platform_data = &bn_led_data,
	},
};

enum {
	HOME_KEY_INDEX = 0,
	HALL_SENSOR_INDEX,
};

/* GPIO_KEY for Tablet */
static struct gpio_keys_button bn_gpio_keys_buttons[] = {
	[HOME_KEY_INDEX] = {
		.code			= KEY_HOME,
		.gpio			= 32,
		.desc			= "SW1",
		.active_low		= 1,
		.wakeup			= 1,
	},
	[HALL_SENSOR_INDEX] = {
		.code			= SW_LID,
		.type			= EV_SW,
		.gpio			= 31,
		.desc			= "HALL",
		.active_low		= 1,
		.wakeup			= 1,
		.debounce_interval = 5,
	},
};

static void gpio_key_buttons_mux_init(void)
{
	/* Hall sensor */
	omap_mux_init_gpio(31, OMAP_PIN_INPUT |
			OMAP_PIN_OFF_WAKEUPENABLE);
}

static struct gpio_keys_platform_data bn_gpio_keys = {
	.buttons	= bn_gpio_keys_buttons,
	.nbuttons	= ARRAY_SIZE(bn_gpio_keys_buttons),
	.rep		= 0,
};

static struct platform_device bn_gpio_keys_device = {
	.name		= "gpio-keys",
	.id			= -1,
	.dev = {
		.platform_data = &bn_gpio_keys,
	},
};

static struct platform_device *bn_devices[] __initdata = {
	&bn_leds_gpio,
	&bn_gpio_keys_device,
};

int __init bn_button_init(void)
{
	int status;

	gpio_key_buttons_mux_init();

	platform_add_devices(bn_devices, ARRAY_SIZE(bn_devices));

	status = omap4_keyboard_init(&bn_keypad_data, &keypad_data);
	if (status)
		pr_err("Keypad initialization failed: %d\n", status);

	return 0;
}
