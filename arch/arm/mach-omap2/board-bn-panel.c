/*
 * arch/arm/mach-omap2/board-bn-panel.c
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

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/leds-omap4430sdp-display.h>
#include <linux/lp855x.h>
#include <linux/platform_device.h>
#include <linux/omapfb.h>
#include <video/omapdss.h>
#include <linux/regulator/consumer.h>
#include <linux/memblock.h>
#include <linux/i2c/twl.h>
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
#include <linux/i2c/maxim9606.h>
#endif

#include <video/omap-panel-dsi.h>
#ifdef CONFIG_PANEL_HYDIS
#include <video/omap-panel-hydis.h>
#endif

#include <asm/system_info.h>
#include <asm/mach-types.h>

#include <plat/vram.h>
#include <plat/android-display.h>

#include "board-bn-hd.h"
#include "control.h"
#include "mux.h"

#define LCD_BL_PWR_EN_GPIO		38
#define LCD_DCR_1V8_GPIO_EVT1B	27

#ifdef CONFIG_MACH_OMAP_OVATION
#define LCD_DCR_1V8_GPIO		153
/* For board version >= preevt1b */
#define LCD_CM_EN				145

#define auo_enable_dpi			lg_enable_dsi
#define auo_disable_dpi			lg_disable_dsi
#endif

#define INITIAL_BRT				0x3F
#define MAX_BRT					0xFF

#define HDMI_GPIO_CT_CP_HPD		60
#define HDMI_GPIO_HPD			63  /* Hot plug pin for HDMI */
#define HDMI_GPIO_LS_OE			81  /* Level shifter for HDMI */
#define GPIO_UART_DDC_SWITCH	182

#define HDMI_DDC_SCL_PULLUPRESX	24
#define HDMI_DDC_SDA_PULLUPRESX	28
#define HDMI_DDC_DISABLE_PULLUP	1
#define HDMI_DDC_ENABLE_PULLUP	0

#define TWL6030_TOGGLE3			0x92
#define LED_PWM1ON				0x00
#define LED_PWM1OFF				0x01

int Vdd_LCD_CT_PEN_request_supply(struct device *dev, const char *supply_name);
int Vdd_LCD_CT_PEN_enable(struct device *dev, const char *supply_name);
int Vdd_LCD_CT_PEN_disable(struct device *dev, const char *supply_name);
int Vdd_LCD_CT_PEN_release_supply(struct device *dev, const char *supply_name);

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
static char display[51];
static int __init get_display_vendor(char *str)
{
	strncpy(display, str, sizeof(display));
	display[sizeof(display) - 1] = '\0';
	return 0;
}
early_param("display.vendor", get_display_vendor);
#endif

static struct regulator *bn_bl_i2c_pullup_power;
static bool lcd_supply_requested = false;
static bool first_boot = true;

struct omap_tablet_panel_data {
	struct omap_dss_board_info *board_info;
	struct dsscomp_platform_data *dsscomp_data;
	struct sgx_omaplfb_platform_data *omaplfb_data;
};

struct lp855x_rom_data bn_bl_rom_data[] = {
#ifdef CONFIG_MACH_OMAP_OVATION
	/*reg 0xA7 bits [1..0] set boost current limit to 1,2A */
	{
		.addr = 0xA7,
		.val  = 0xFD,
	},
	{
		.addr = 0xA9,
		.val  = 0x80
	},
	{
		.addr = 0xA4,
		.val  = 0x02
	},
#endif
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	{
		.addr = 0xA9,
		.val  = 0x60
	},
#endif
};

static int bn_lcd_request_resources(void)
{
	/* Request the display power supply */
	int ret = Vdd_LCD_CT_PEN_request_supply(NULL, "vlcd");

	if(ret)
		pr_err("%s: Could not get lcd supply\n", __func__);

	return ret;
}

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
static int bn_lcd_release_resources(void)
{
	/* Release the display power supply */
	int ret = Vdd_LCD_CT_PEN_release_supply(NULL, "vlcd");

	if(ret)
		pr_err("%s: Could not release lcd supply\n", __func__);

	return ret;
}
#endif

static int bn_lcd_enable_supply(void)
{
	int ret = 0;

	// on the first boot the LCD regulator was enabled earlier
	if (!first_boot) {
		if(!lcd_supply_requested) {
			bn_lcd_request_resources();
			lcd_supply_requested = true;
		}

		ret = Vdd_LCD_CT_PEN_enable(NULL, "vlcd");

		msleep(100); // T2 timing
	} else
		first_boot = false;

	return ret;
}

static int bn_lcd_disable_supply(void)
{
	return Vdd_LCD_CT_PEN_disable(NULL, "vlcd");
}

static int bn_bl_request_resources(struct device *dev)
{
	int ret = gpio_request(LCD_BL_PWR_EN_GPIO, "BL-PWR-EN");

	if (ret) {
		pr_err("Cannot request backlight power enable gpio");
		return -EBUSY;
	}

#ifdef CONFIG_MACH_OMAP_OVATION
	gpio_direction_output(LCD_BL_PWR_EN_GPIO, 1);
#endif

	if (!bn_bl_i2c_pullup_power)
		bn_bl_i2c_pullup_power = regulator_get(NULL, "bl_i2c_pup");

	if (IS_ERR(bn_bl_i2c_pullup_power)) {
		pr_err("%s: failed to get regulator bl_i2c_pup", __func__);
		gpio_free(LCD_BL_PWR_EN_GPIO);
		return -EBUSY;
	}

	/* enable the LCD regulator early on first boot, originally to
	   maintain boot splash image, which we don't support anymore */
	if (first_boot) {
		if(!lcd_supply_requested) {
			bn_lcd_request_resources();
			lcd_supply_requested = true;
		}

		ret = Vdd_LCD_CT_PEN_enable(NULL, "vlcd");

		msleep(15);
	}

	return ret;
}

static int bn_bl_release_resources(struct device *dev)
{
	gpio_free(LCD_BL_PWR_EN_GPIO);
	regulator_put(bn_bl_i2c_pullup_power);

	return 0;
}

static int bn_bl_enable_supply(void)
{
	int ret = -EBUSY;

	if (!IS_ERR(bn_bl_i2c_pullup_power)) {
		ret = regulator_enable(bn_bl_i2c_pullup_power);
		if (ret)
			pr_err("%s:Could not enable bl i2c-3 pullup power regulator\n",
				__func__);
	} else
		pr_err("%s: touch power regulator is not valid\n", __func__);

	return ret;
}

static int bn_bl_disable_supply(void)
{
	if(!IS_ERR(bn_bl_i2c_pullup_power))
		regulator_disable(bn_bl_i2c_pullup_power);
	else {
		pr_err("%s: touch vdd regulator is not valid\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int bn_bl_power_on(struct device *dev)
{
	int ret;

#ifdef CONFIG_MACH_OMAP_OVATION
	msleep(200);
#endif

	ret = bn_bl_enable_supply();

	twl_i2c_write_u8(TWL_MODULE_PWM, 0x7F, LED_PWM1ON);
	twl_i2c_write_u8(TWL_MODULE_PWM, 0XFF, LED_PWM1OFF);
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x06, TWL6030_TOGGLE3);

	msleep(20); // T14 timing

	gpio_set_value(LCD_BL_PWR_EN_GPIO, 1);

#ifdef CONFIG_MACH_OMAP_OVATION
	msleep(100);
#endif

	return ret;
}

static int bn_bl_power_off(struct device *dev)
{
	gpio_set_value(LCD_BL_PWR_EN_GPIO, 0);

	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x01, TWL6030_TOGGLE3);
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x07, TWL6030_TOGGLE3);

	bn_bl_disable_supply();

	msleep(200); // T6 timing

	return 0;
}

static struct lp855x_platform_data lp8556_pdata = {
	.name = "lcd-backlight",
	.mode = REGISTER_BASED,
	.device_control = LP8556_COMB1_CONFIG
					| (machine_is_omap_hummingbird() ? LP8556_FAST_CONFIG : 0),
	.initial_brightness = INITIAL_BRT,
	.max_brightness = MAX_BRT,
	.led_setting = (machine_is_omap_ovation() ? PS_MODE_5P5D : PS_MODE_4P4D)
													| PWM_FREQ6916HZ,
	.boost_freq = BOOST_FREQ625KHZ,
	.nonlinearity_factor = 30,
	.load_new_rom_data = 1,
	.size_program = machine_is_omap_ovation() ? 2 : 1,
	.rom_data = bn_bl_rom_data,
	.request_resources = bn_bl_request_resources,
	.release_resources = bn_bl_release_resources,
	.power_on = bn_bl_power_on,
	.power_off = bn_bl_power_off,
};

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
static void _enable_supplies(ulong delay)
{
	bool safemode = false;

	if (!regulator_is_enabled(bn_bl_i2c_pullup_power)) {
		omap_mux_init_signal("i2c3_scl.safe_mode", OMAP_PIN_INPUT);
		omap_mux_init_signal("i2c3_sda.safe_mode", OMAP_PIN_INPUT);
		safemode = true;
	}

	// enable i2c level shifter so tcon can talk to eeprom
	bn_bl_enable_supply();

	msleep(6);

	bn_lcd_enable_supply();

	msleep(delay);

	if (safemode) {
		omap_mux_init_signal("i2c3_scl.i2c3_scl", OMAP_PIN_INPUT);
		omap_mux_init_signal("i2c3_sda.i2c3_sda", OMAP_PIN_INPUT);
	}
}

static void _disable_supplies(void)
{
	bn_bl_disable_supply();

	msleep(10);

	bn_lcd_disable_supply();
}

static int lg_maxim9606_power_on(struct device *dev)
{
	_enable_supplies(74);
	return 0;
}

static int lg_maxim9606_power_off(struct device *dev)
{
	_disable_supplies();
	return 0;
}

static int lg_maxim9606_request_resources(struct device *dev)
{
	int ret = bn_lcd_request_resources();
	
	if(ret)
		return ret;

	return bn_bl_request_resources(dev);
}

static int lg_maxim9606_release_resources(struct device *dev)
{
	int ret = bn_lcd_release_resources();
	
	if(ret)
		return ret;

	ret = bn_bl_release_resources(dev);

	omap_mux_init_signal("i2c3_scl.safe_mode", OMAP_PIN_INPUT);
	omap_mux_init_signal("i2c3_sda.safe_mode", OMAP_PIN_INPUT);

	return ret;
}

static struct maxim9606_platform_data maxim9606_pdata = {
	.power_on 	= lg_maxim9606_power_on,
	.power_off 	= lg_maxim9606_power_off,
	.request_resources	= lg_maxim9606_request_resources,
	.release_resources	= lg_maxim9606_release_resources,
};

static struct i2c_board_info __initdata maxim9606_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("maxim9606", 0x74),
		.platform_data = &maxim9606_pdata,
	},
};
#endif

static struct i2c_board_info __initdata lp8556_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("lp8556", 0x2C),
		.platform_data = &lp8556_pdata,
	},
};

static struct omap_dss_hdmi_data bn_hdmi_data = {
	.hpd_gpio		= HDMI_GPIO_HPD,
	.ct_cp_hpd_gpio	= HDMI_GPIO_CT_CP_HPD,
	.ls_oe_gpio		= HDMI_GPIO_LS_OE,
};

static struct dsscomp_platform_data dsscomp_config = {
	.tiler1d_slotsz = machine_is_omap_ovation() ?
					  (SZ_16M + SZ_16M + SZ_2M) :
					  (SZ_16M + SZ_2M + SZ_8M + SZ_1M),
};

static struct sgx_omaplfb_config omaplfb_config[] = {
	{
		.vram_buffers = 4,
		.swap_chain_length = 2,
	},
#if defined(CONFIG_OMAP4_DSS_HDMI)
	{
	.vram_buffers = 2,
	.swap_chain_length = 2,
	},
#endif
};

static void __init bn_hdmi_mux_init(void)
{
	u32 r;
	/* PAD0_HDMI_HPD_PAD1_HDMI_CEC */
	omap_mux_init_signal("hdmi_hpd.hdmi_hpd",
				OMAP_PIN_INPUT_PULLDOWN);
	omap_mux_init_signal("gpmc_wait2.gpio_100",
			OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("hdmi_cec.hdmi_cec",
			OMAP_PIN_INPUT_PULLUP);
	/* PAD0_HDMI_DDC_SCL_PAD1_HDMI_DDC_SDA */
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	if (system_rev > HUMMINGBIRD_EVT0) {
#endif
#ifdef CONFIG_MACH_OMAP_OVATION
	if (system_rev > OVATION_EVT1A) {
#endif
		omap_mux_init_signal("hdmi_ddc_scl.safe_mode",
			OMAP_PIN_INPUT);
		omap_mux_init_signal("hdmi_ddc_sda.safe_mode",
			OMAP_PIN_INPUT);
	} else {
		omap_mux_init_signal("hdmi_ddc_scl.hdmi_ddc_scl",
			OMAP_PIN_INPUT);
		omap_mux_init_signal("hdmi_ddc_sda.hdmi_ddc_sda",
			OMAP_PIN_INPUT);
	}

#ifdef CONFIG_MACH_OMAP_OVATION
	if (system_rev >= OVATION_EVT1B) {
		omap_mux_init_signal("sdmmc5_clk.gpio_145",
			OMAP_PIN_OUTPUT);
		omap_mux_init_signal("dpm_emu16.gpio_27",
			OMAP_PIN_OUTPUT);
	} else {
		/* EVT1a compatibility */
		omap_mux_init_signal("mcspi4_somi.gpio_153",
			OMAP_PIN_OUTPUT);
		omap_mux_init_signal("usbb2_ulpitll_stp.dispc2_data23",
			OMAP_PIN_INPUT);
	}
#endif

	/* Disable strong pullup on DDC lines using unpublished register */
	r = ((HDMI_DDC_DISABLE_PULLUP << HDMI_DDC_SCL_PULLUPRESX) |
		(HDMI_DDC_DISABLE_PULLUP << HDMI_DDC_SDA_PULLUPRESX));
	omap4_ctrl_pad_writel(r, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_1);

	omap_mux_init_gpio(HDMI_GPIO_LS_OE, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(HDMI_GPIO_CT_CP_HPD, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(HDMI_GPIO_HPD, OMAP_PIN_INPUT_PULLDOWN);
}

static void __init bn_lcd_init(void)
{
	u32 reg;

#ifdef CONFIG_MACH_OMAP_OVATION
	if (system_rev < OVATION_EVT1B) {
		gpio_request(LCD_DCR_1V8_GPIO, "lcd_dcr");
		gpio_direction_output(LCD_DCR_1V8_GPIO, 1);
		return;
	} else {
		gpio_request(LCD_CM_EN, "lcd_cm_en");
	}
#endif

	gpio_request(LCD_DCR_1V8_GPIO_EVT1B, "lcd_dcr");

	/* Enable 5 lanes in DSI1 module, disable pull down */
	reg = omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_DSIPHY);
	reg &= ~OMAP4_DSI1_LANEENABLE_MASK;
	reg |= 0x1f << OMAP4_DSI1_LANEENABLE_SHIFT;
	reg &= ~OMAP4_DSI1_PIPD_MASK;
	reg |= 0x1f << OMAP4_DSI1_PIPD_SHIFT;
	omap4_ctrl_pad_writel(reg, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_DSIPHY);
}

static int lg_enable_dsi(struct omap_dss_device *dssdev)
{
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	_enable_supplies(14);
#endif
#ifdef CONFIG_MACH_OMAP_OVATION
	bn_lcd_enable_supply();
	gpio_direction_output(LCD_CM_EN, 1);
#endif

	gpio_direction_output(LCD_DCR_1V8_GPIO_EVT1B, 1);

	return 0;
}

static void lg_disable_dsi(struct omap_dss_device *dssdev)
{
	gpio_direction_output(LCD_DCR_1V8_GPIO_EVT1B, 0);

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	msleep(100);
	_disable_supplies();
#endif
#ifdef CONFIG_MACH_OMAP_OVATION
	gpio_direction_output(LCD_CM_EN, 0);
	bn_lcd_disable_supply();
#endif
}

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
static int auo_enable_dsi(struct omap_dss_device *dssdev)
{

	_enable_supplies(194);
	gpio_direction_output(LCD_DCR_1V8_GPIO_EVT1B, 0);

	return 0;
}

static void auo_disable_dsi(struct omap_dss_device *dssdev)
{
	gpio_direction_output(LCD_DCR_1V8_GPIO_EVT1B, 0);
	msleep(100);
	_disable_supplies();
}
#endif

static int bn_enable_hdmi(struct omap_dss_device *dssdev)
{
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	if (system_rev > HUMMINGBIRD_EVT0) {
#endif
#ifdef CONFIG_MACH_OMAP_OVATION
	if (system_rev > OVATION_EVT1A) {
#endif
		omap_mux_init_signal("hdmi_ddc_scl.hdmi_ddc_scl",
			OMAP_PIN_INPUT);
		omap_mux_init_signal("hdmi_ddc_sda.hdmi_ddc_sda",
			OMAP_PIN_INPUT);

		omap_mux_init_signal("i2c2_scl.safe_mode",
			OMAP_PIN_INPUT);
		omap_mux_init_signal("i2c2_sda.safe_mode",
			OMAP_PIN_INPUT);
	} else
		gpio_direction_output(GPIO_UART_DDC_SWITCH, 0);

	return 0;
}

static void bn_disable_hdmi(struct omap_dss_device *dssdev)
{
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	if (system_rev > HUMMINGBIRD_EVT0) {
#endif
#ifdef CONFIG_MACH_OMAP_OVATION
	if (system_rev > OVATION_EVT1A) {
#endif
		omap_mux_init_signal("hdmi_ddc_scl.safe_mode",
			OMAP_PIN_INPUT);
		omap_mux_init_signal("hdmi_ddc_sda.safe_mode",
			OMAP_PIN_INPUT);

		omap_mux_init_signal("i2c2_sda.uart1_tx",
			OMAP_PIN_INPUT);
		omap_mux_init_signal("i2c2_scl.uart1_rx",
			OMAP_PIN_INPUT | OMAP_PIN_OFF_WAKEUPENABLE |
			OMAP_PIN_OFF_INPUT_PULLUP);
	} else
		gpio_direction_output(GPIO_UART_DDC_SWITCH, 1);
}

#ifdef CONFIG_MACH_OMAP_OVATION
static int pwm_level;

static int ovation_set_pwm_bl(struct omap_dss_device *dssdev, int level)
{
	u8 brightness = 0;

	if (level) {
		if (level >= 255) {
			brightness = 0x7f;
		} else {
			brightness = (~(level/2)) & 0x7f;
		}

		twl_i2c_write_u8(TWL_MODULE_PWM, brightness, LED_PWM1ON);
		twl_i2c_write_u8(TWL_MODULE_PWM, 0XFF, LED_PWM1OFF);
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x06, TWL6030_TOGGLE3);
	} else {
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x01, TWL6030_TOGGLE3);
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x07, TWL6030_TOGGLE3);
	}

	pwm_level = level;
	return 0;
}

static int ovation_get_pwm_bl(struct omap_dss_device *dssdev)
{
	return pwm_level;
}

static struct omap_dss_device bn_lcd_auo = {
	.name					= "auo_lcd",
	.driver_name			= "auo",
	.type					= OMAP_DISPLAY_TYPE_DPI,
	.data					= NULL,
	.phy.dpi.data_lines		= 24,
	.channel				= OMAP_DSS_CHANNEL_LCD2,
	.platform_enable		= auo_enable_dpi,
	.platform_disable		= auo_disable_dpi,
	.clocks	= {
		.dispc	= {
			.dispc_fclk_src	= OMAP_DSS_CLK_SRC_FCK,
		},
		.hdmi	= {
			.regn	= 15,
			.regm2	= 1,
			.max_pixclk_khz = 148500,
		},
	},
};
#endif

static struct omap_dss_device bn_lcd_novatek = {
	.name				= "lcd",
	.driver_name		= "novatek-panel",
	.type				= OMAP_DISPLAY_TYPE_DSI,
	.phy.dsi = {
		.clk_lane		= 1,
		.clk_pol		= 0,
		.data1_lane		= 2,
		.data1_pol		= 0,
		.data2_lane		= 3,
		.data2_pol		= 0,
		.data3_lane		= 4,
		.data3_pol		= 0,
		.data4_lane		= 5,
		.data4_pol		= 0,

		.module			= 0,
	},

	.clocks = {
		.dispc = {
			 .channel = {
				.lck_div	= 1,
				.pck_div	= 1,
				.lcd_clk_src = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_FCK,
		},

		.dsi = {
#ifdef CONFIG_MACH_OMAP_OVATION
			.regn			= 20,
			.regm			= 348,
			.regm_dispc		= 9,
			.regm_dsi		= 8,
			.lp_clk_div		= 16,
#endif
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
			.regn			= 24,
			.regm			= 250,
			.regm_dispc		= 9,
			.regm_dsi		= 5,
			.lp_clk_div		= 8,
#endif
			.offset_ddr_clk	= 0,
			.dsi_fclk_src	= OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
		},
	},

	.ctrl = {
		.pixel_size			= 18,
		.dither				= true,
	},

	.panel = {
		.timings = {
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
			.x_res			= 900,
			.y_res			= 1440,
			.pixel_clock	= 88888,
			.hfp			= 32,
			.hsw			= 28,
			.hbp			= 48,
			.vfp			= 14,
			.vsw			= 18,
			.vbp			= 3,
#endif
#ifdef CONFIG_MACH_OMAP_OVATION
			.x_res			= 1920,
			.y_res			= 1280,
			.pixel_clock	= 148480,
			.hfp			= 4,
			.hsw			= 5,
			.hbp			= 39,
			.vfp			= 9,
			.vsw			= 1,
			.vbp			= 10,
#endif
		},
		.dsi_mode = OMAP_DSS_DSI_VIDEO_MODE,
		.dsi_vm_data = {
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
			.hsa		= 0,
			.hfp		= 17,
			.hbp		= 41,
			.vsa		= 18,
			.vbp		= 3,
			.vfp		= 14,
#endif
#ifdef CONFIG_MACH_OMAP_OVATION
			.hsa		= 0,
			.hfp		= 24,
			.hbp		= 0,
			.vsa		= 1,
			.vbp		= 9,
			.vfp		= 10,
#endif
			/* DSI blanking modes */
			.blanking_mode		= 0,
			.hsa_blanking_mode	= 1,
			.hbp_blanking_mode	= 1,
			.hfp_blanking_mode	= 1,

			.vp_de_pol			= 1,
			.vp_vsync_pol		= 1,
			.vp_hsync_pol		= 0,
			.vp_hsync_end		= 0,
			.vp_vsync_end		= 0,

			.ddr_clk_always_on	= 0,
			.window_sync		= 4,
		},
#if 0
		.dsi_cio_data = {
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
			.ths_prepare		= 16,
			.ths_prepare_ths_zero = 21,
			.ths_trail			= 17,
			.ths_exit			= 29,
			.tlpx_half			= 5,
			.tclk_trail			= 14,
			.tclk_zero			= 53,
			.tclk_prepare		= 13,
			.reg_ttaget			= 4,
#endif
#ifdef CONFIG_MACH_OMAP_OVATION
			.ths_prepare		= 26,
			.ths_prepare_ths_zero = 61,
			.ths_trail			= 26,
			.ths_exit			= 49,
			.tlpx_half			= 8,
			.tclk_trail			= 23,
			.tclk_zero			= 89,
			.tclk_prepare		= 22,
			.reg_ttaget			= 4,
#endif
		},
#endif
		.acbi 			= 40,
		.acb			= 0,
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
		.width_in_um	= 94230,
		.height_in_um	= 150770,
#endif
#ifdef CONFIG_MACH_OMAP_OVATION
		.width_in_um	= 190080,
		.height_in_um	= 126720,
#endif
	},

	.channel = OMAP_DSS_CHANNEL_LCD,
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	.skip_init = true,
#else
	.skip_init = false,
#endif
	.platform_enable = lg_enable_dsi,
	.platform_disable = lg_disable_dsi,
#ifdef CONFIG_MACH_OMAP_OVATION
	.set_backlight = ovation_set_pwm_bl,
	.get_backlight = ovation_get_pwm_bl,
#endif
};

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
static struct omap_dss_device bn_lcd_orise = {
	.name                   = "lcd",
	.driver_name            = "orise-panel",
	.type                   = OMAP_DISPLAY_TYPE_DSI,
	.phy.dsi                = {
		.clk_lane       = 1,
		.clk_pol        = 0,
		.data1_lane     = 2,
		.data1_pol      = 0,
		.data2_lane     = 3,
		.data2_pol      = 0,
		.data3_lane     = 4,
		.data3_pol      = 0,
		.data4_lane     = 5,
		.data4_pol      = 0,

		.module		= 0,
	},

	.clocks = {
		.dispc = {
			 .channel = {
				.lck_div        = 1,
				.pck_div        = 1,
				.lcd_clk_src    = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_FCK,
		},

		.dsi = {
			.regn           = 24,
			.regm           = 260,
			.regm_dispc     = 9,
			.regm_dsi       = 5,
			.lp_clk_div     = 9,
			.offset_ddr_clk = 0,
			.dsi_fclk_src   = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
		},
	},

	.ctrl = {
		.pixel_size 	= 18,
		.dither		= true,
	},

	.panel = {
		.timings = {
			.x_res		= 900,
			.y_res		= 1440,
			.pixel_clock 	= 92444,
			.hfp		= 76,
			.hsw		= 40,
			.hbp		= 40,
			.vfp		= 10,
			.vsw		= 9,
			.vbp		= 1,
		},
		.dsi_mode = OMAP_DSS_DSI_VIDEO_MODE,
		.dsi_vm_data = {
			.hsa		= 0,
			.hfp		= 42,
			.hbp		= 43,
			.vsa		= 9,
			.vbp		= 1,
			.vfp		= 10,

			/* DSI blanking modes */
			.blanking_mode		= 1,
			.hsa_blanking_mode	= 1,
			.hbp_blanking_mode	= 1,
			.hfp_blanking_mode	= 1,

			.vp_de_pol		= 1,
			.vp_vsync_pol		= 1,
			.vp_hsync_pol		= 0,
			.vp_hsync_end		= 0,
			.vp_vsync_end		= 0,

			.ddr_clk_always_on	= 0,
			.window_sync		= 4,
		},
#if 0
		.dsi_cio_data = {
			.ths_prepare		= 17,
			.ths_prepare_ths_zero	= 22,
			.ths_trail		= 17,
			.ths_exit		= 31,
			.tlpx_half		= 5,
			.tclk_trail		= 15,
			.tclk_zero		= 55,
			.tclk_prepare		= 14,
			.reg_ttaget		= 4,
		},
#endif
		.acbi 		= 40,
		.acb		= 0,
		.width_in_um	= 94230,
		.height_in_um	= 150770,
	},


	.channel = OMAP_DSS_CHANNEL_LCD,
#ifdef CONFIG_FB_OMAP_BOOTLOADER_INIT
	.skip_init = true,
#else
	.skip_init = false,
#endif

	.platform_enable = auo_enable_dsi,
	.platform_disable = auo_disable_dsi,
};
#endif

static struct omap_dss_device bn_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.clocks	= {
		.dispc	= {
			.dispc_fclk_src	= OMAP_DSS_CLK_SRC_FCK,
		},
		.hdmi	= {
			.regn	= 15,
			.regm2	= 1,
			.max_pixclk_khz = 148500,
		},
	},
	.data			= &bn_hdmi_data,
	.channel		= OMAP_DSS_CHANNEL_DIGIT,
	.platform_enable	= bn_enable_hdmi,
	.platform_disable	= bn_disable_hdmi,
};

static struct omap_dss_device *bn_dss_devices[] = {
	&bn_lcd_novatek,
	&bn_hdmi_device,
};

static struct omap_dss_board_info bn_dss_data = {
	.num_devices	= ARRAY_SIZE(bn_dss_devices),
	.devices		= bn_dss_devices,
	.default_device	= &bn_lcd_novatek,
};

static struct sgx_omaplfb_platform_data omaplfb_plat_data = {
	.num_configs = ARRAY_SIZE(omaplfb_config),
	.configs = omaplfb_config,
};

static struct omap_tablet_panel_data bn_panel_data = {
	.board_info = &bn_dss_data,
	.dsscomp_data = &dsscomp_config,
	.omaplfb_data = &omaplfb_plat_data,
};

static struct omapfb_platform_data bn_fb_pdata = {
	.mem_desc = {
		.region_cnt = ARRAY_SIZE(omaplfb_config),
	},
};

void bn_android_display_setup(void)
{
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	if ((system_rev >= HUMMINGBIRD_EVT0B) && !strncmp(display, "AUO", 3)) {
		bn_dss_devices[0] = &bn_lcd_orise;
		bn_dss_data.default_device = &bn_lcd_orise;
	}
#endif

#ifdef CONFIG_MACH_OMAP_OVATION
	if (system_rev < OVATION_EVT1B) {
		bn_dss_devices[0] = &bn_lcd_auo;
		bn_dss_data.default_device = &bn_lcd_auo;
	}
#endif

	omap_android_display_setup(bn_panel_data.board_info,
				bn_panel_data.dsscomp_data,
				bn_panel_data.omaplfb_data,
				&bn_fb_pdata);
}

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
static void __init hummingbird_lcd_mux_init(void)
{
	omap_mux_init_signal("sdmmc5_dat2.mcspi2_cs1",
				OMAP_PIN_OUTPUT);
//	omap_mux_init_signal("sdmmc5_dat0.mcspi2_somi",
//				OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("sdmmc5_cmd.mcspi2_simo",
				OMAP_PIN_OUTPUT);
	omap_mux_init_signal("sdmmc5_clk.mcspi2_clk",
				OMAP_PIN_OUTPUT);

	/* reset mipi to lvds bridge */
	omap_mux_init_signal("sdmmc5_dat0.gpio_147",
				OMAP_PIN_OUTPUT);
	omap_mux_init_signal("dpm_emu16.gpio_27",
			OMAP_PIN_OUTPUT);
}
#endif

int __init bn_panel_init(void)
{
	int bus_i = 3;

	bn_lcd_init();
	bn_hdmi_mux_init();

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	hummingbird_lcd_mux_init();
#endif

	omapfb_set_platform_data(&bn_fb_pdata);

	omap_display_init(&bn_dss_data);

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	if (strncmp(display, "AUO", 3))
		i2c_register_board_info(3, maxim9606_i2c_boardinfo,
						ARRAY_SIZE(maxim9606_i2c_boardinfo));
#endif

#ifdef CONFIG_MACH_OMAP_OVATION
	if (system_rev < OVATION_EVT1B)
		bus_i = 1;
#endif

	i2c_register_board_info(bus_i, lp8556_i2c_boardinfo,
						ARRAY_SIZE(lp8556_i2c_boardinfo));

	return 0;
}
