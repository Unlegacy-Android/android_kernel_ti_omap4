/*
 * arch/arm/mach-omap2/board-44xx-tablet2-touch.c
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <plat/i2c.h>
#include <linux/input/ft5x06_ts.h>

#include "board-acclaim.h"
#include "mux.h"


#define FT5x06_I2C_SLAVEADDRESS  (0x70 >> 1)
#define OMAP_FT5x06_GPIO         37 /*99*/
#define OMAP_FT5x06_RESET_GPIO   39 /*46*/

#if 0
int ft5x06_dev_init(int resource)
{
        if (resource){
                omap_mux_init_signal("gpmc_ad13.gpio_37", OMAP_PIN_INPUT | OMAP_PIN_OFF_WAKEUPENABLE);
                omap_mux_init_signal("gpmc_ad15.gpio_39", OMAP_PIN_OUTPUT );

                if (gpio_request(OMAP_FT5x06_RESET_GPIO, "ft5x06_reset") < 0){
                        printk(KERN_ERR "can't get ft5x06 xreset GPIO\n");
                        return -1;
                }

                if (gpio_request(OMAP_FT5x06_GPIO, "ft5x06_touch") < 0) {
                        printk(KERN_ERR "can't get ft5x06 interrupt GPIO\n");
                        return -1;
                }

                gpio_direction_input(OMAP_FT5x06_GPIO);
        } else {
                gpio_free(OMAP_FT5x06_GPIO);
                gpio_free(OMAP_FT5x06_RESET_GPIO);
        }

        return 0;
}

static void ft5x06_platform_suspend(void)
{
//        omap_mux_init_signal("gpmc_ad13.gpio_37", OMAP_PIN_INPUT );
}

static void ft5x06_platform_resume(void)
{
//        omap_mux_init_signal("gpmc_ad13.gpio_37", OMAP_PIN_INPUT | OMAP_PIN_OFF_WAKEUPENABLE);
}
#endif

static inline int ft5x06_power_stub(bool on) { return 0; }

static struct ft5x06_ts_platform_data ft5x06_platform_data = {
        .irqflags = IRQF_TRIGGER_FALLING,
        .irq_gpio = OMAP_FT5x06_GPIO,
        .irq_gpio_flags = GPIOF_IN,
        .reset_gpio = OMAP_FT5x06_RESET_GPIO,
        .reset_gpio_flags = GPIOF_OUT_INIT_LOW,
        .x_max = 600,
        .y_max = 1024,
        .ignore_id_check = true,
        .power_init = ft5x06_power_stub,
        .power_on = ft5x06_power_stub,
#if 0 // AM: old pdata left here mostly for reference
        .maxx = 600,
        .maxy = 1024,
        .flags = 0, // FLIP_DATA_FLAG, // | REVERSE_Y_FLAG,
        .use_st = FT_USE_ST,
        .use_mt = FT_USE_MT,
        .use_trk_id = FT_USE_TRACKING_ID,
        .use_sleep = FT_USE_SLEEP,
        .use_gestures = 1,
//      .platform_suspend = ft5x06_platform_suspend,
//      .platform_resume = ft5x06_platform_resume,
#endif
};

static struct i2c_board_info __initdata sdp4430_i2c_2_boardinfo[] = {
        {
                I2C_BOARD_INFO("ft5x06_ts", FT5x06_I2C_SLAVEADDRESS),
                .platform_data = &ft5x06_platform_data,
                .irq = OMAP_GPIO_IRQ(OMAP_FT5x06_GPIO),
        },
};


int __init acclaim_touch_init(void)
{

       i2c_register_board_info(2, sdp4430_i2c_2_boardinfo,
             ARRAY_SIZE(sdp4430_i2c_2_boardinfo));
//        omap_register_i2c_bus(2, 400, &sdp4430_i2c_2_bus_pdata,
  //                      sdp4430_i2c_2_boardinfo, ARRAY_SIZE(sdp4430_i2c_2_boardinfo));

	return 0;
}
