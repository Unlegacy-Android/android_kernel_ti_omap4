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

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/lp855x.h>

#include <asm/system_info.h>
#include <asm/mach-types.h>

#include <plat/android-display.h>

#include <video/omapdss.h>
#include <video/omap-panel-dsi.h>
#ifdef CONFIG_PANEL_HYDIS
#include <video/omap-panel-hydis.h>
#endif

#include "mux.h"
#include "control.h"
#include "board-bn-hd.h"

#define LCD_BL_PWR_EN_GPIO		38
#define LCD_DCR_1V8_GPIO_EVT1B	27

#ifdef CONFIG_MACH_OMAP_OVATION
#define LCD_DCR_1V8_GPIO		153
/* For board version >= preevt1b */
#define LCD_CM_EN				145
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

static int bn_bl_power_init(bool enable)
{
	if (enable) {
		twl_i2c_write_u8(TWL_MODULE_PWM, 0x7F, LED_PWM1ON);
		twl_i2c_write_u8(TWL_MODULE_PWM, 0xFF, LED_PWM1OFF);
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x06, TWL6030_TOGGLE3);
	} else {
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x01, TWL6030_TOGGLE3);
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x07, TWL6030_TOGGLE3);
	}

	return 0;
}

static struct lp855x_platform_data lp8556_pdata = {
	.name = "lcd-backlight",
	.mode = REGISTER_BASED,
	.device_control = LP8556_COMB1_CONFIG
					| (machine_is_omap_hummingbird() ? LP8556_FAST_CONFIG : 0),
	.initial_brightness = INITIAL_BRT,
	.power_init = bn_bl_power_init,
	.led_setting = (machine_is_omap_ovation() ? PS_MODE_5P5D : PS_MODE_4P4D)
													| PWM_FREQ6916HZ,
	.boost_freq = BOOST_FREQ625KHZ,
	.nonlinearity_factor = 30,
	.load_new_rom_data = 1,
	.size_program = machine_is_omap_ovation() ? 2 : 1,
	.rom_data = bn_bl_rom_data,
	.gpio_en = LCD_BL_PWR_EN_GPIO,
	.regulator_name = "bl_i2c_pup",
#if 0 // AM: old pdata left here mostly for reference
	.request_resources = bn_bl_request_resources,
	.release_resources = bn_bl_release_resources,
	.power_on = bn_bl_power_on,
	.power_off = bn_bl_power_off,
	.max_brightness = MAX_BRT,
#endif
};

static struct i2c_board_info __initdata lp8556_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("lp8556", 0x2C),
		.platform_data = &lp8556_pdata,
	},
};

static struct panel_board_data bn_lcd_data = {
	.lcd_dcr_gpio	= LCD_DCR_1V8_GPIO_EVT1B,
#ifdef CONFIG_MACH_OMAP_OVATION
	.lcd_cm_gpio	= LCD_CM_EN,
#endif
	.regulator_name	= "vlcd",
};

static struct omap_dss_device bn_lcd_panel = {
	.name				= "lcd",
	.driver_name		= "novatek-panel",
	.type				= OMAP_DISPLAY_TYPE_DSI,
	.data				= &bn_lcd_data,
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
	.platform_enable = NULL,
	.platform_disable = NULL,
};

#ifdef CONFIG_OMAP4_DSS_HDMI
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

static struct omap_dss_hdmi_data bn_hdmi_data = {
	.hpd_gpio		= HDMI_GPIO_HPD,
	.ct_cp_hpd_gpio	= HDMI_GPIO_CT_CP_HPD,
	.ls_oe_gpio		= HDMI_GPIO_LS_OE,
};

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
#endif

static struct omap_dss_device *bn_dss_devices[] = {
	&bn_lcd_panel,
#ifdef CONFIG_OMAP4_DSS_HDMI
	&bn_hdmi_device,
#endif
};

static struct dsscomp_platform_data dsscomp_config = {
	.tiler1d_slotsz = machine_is_omap_ovation() ?
					  (SZ_16M + SZ_16M + SZ_2M) :
					  (SZ_16M + SZ_2M + SZ_8M + SZ_1M),
};

static struct sgx_omaplfb_config omaplfb_config[] = {
	{
		.vram_buffers = machine_is_omap_hummingbird() ? 0 : 2,
		.tiler2d_buffers = machine_is_omap_ovation() ? 0 : 2,
		.swap_chain_length = 2,
	},
#if defined(CONFIG_OMAP4_DSS_HDMI)
	{
		.vram_buffers = 2,
		.swap_chain_length = 2,
	},
#endif
};

static struct sgx_omaplfb_platform_data omaplfb_plat_data = {
	.num_configs = ARRAY_SIZE(omaplfb_config),
	.configs = omaplfb_config,
};

static struct omap_dss_board_info bn_dss_data = {
	.num_devices	= ARRAY_SIZE(bn_dss_devices),
	.devices		= bn_dss_devices,
	.default_device	= &bn_lcd_panel,
};

static struct omapfb_platform_data bn_fb_pdata = {
	.mem_desc = {
		.region_cnt = ARRAY_SIZE(omaplfb_config),
	},
};

struct omap_tablet_panel_data {
	struct omap_dss_board_info *board_info;
	struct dsscomp_platform_data *dsscomp_data;
	struct sgx_omaplfb_platform_data *omaplfb_data;
};

static struct omap_tablet_panel_data bn_panel_data = {
	.board_info = &bn_dss_data,
	.dsscomp_data = &dsscomp_config,
	.omaplfb_data = &omaplfb_plat_data,
};

static void __init bn_lcd_init(void)
{
	u32 reg;

#ifdef CONFIG_MACH_OMAP_OVATION
	if (system_rev < OVATION_EVT1B)
		return;
#endif

	/* Enable 5 lanes in DSI1 module, disable pull down */
	reg = omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_DSIPHY);
	reg &= ~OMAP4_DSI1_LANEENABLE_MASK;
	reg |= 0x1f << OMAP4_DSI1_LANEENABLE_SHIFT;
	reg &= ~OMAP4_DSI1_PIPD_MASK;
	reg |= 0x1f << OMAP4_DSI1_PIPD_SHIFT;
	omap4_ctrl_pad_writel(reg, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_DSIPHY);
}

void bn_android_display_setup(void)
{
#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	if ((system_rev >= HUMMINGBIRD_EVT0B) && !strncmp(display, "AUO", 3)) {
		bn_lcd_panel.clocks.dsi.regm			= 260;
		bn_lcd_panel.clocks.dsi.lp_clk_div	= 9;

		bn_lcd_panel.panel.timings.pixel_clock	 = 92444;

		bn_lcd_panel.panel.timings.hfp		= 76;
		bn_lcd_panel.panel.timings.hsw		= 40;
		bn_lcd_panel.panel.timings.hbp		= 40;
		bn_lcd_panel.panel.timings.vfp		= 10;
		bn_lcd_panel.panel.timings.vsw		= 9;
		bn_lcd_panel.panel.timings.vbp		= 1;

		bn_lcd_panel.panel.dsi_vm_data.hfp	= 42;
		bn_lcd_panel.panel.dsi_vm_data.hbp	= 43;
		bn_lcd_panel.panel.dsi_vm_data.vsa	= 9;
		bn_lcd_panel.panel.dsi_vm_data.vbp	= 1;
		bn_lcd_panel.panel.dsi_vm_data.vfp	= 10;

		bn_lcd_panel.panel.dsi_vm_data.blanking_mode = 1;
	}
#endif

#ifdef CONFIG_MACH_OMAP_OVATION
	if (system_rev < OVATION_EVT1B) {
		bn_lcd_data.lcd_en_gpio		= LCD_DCR_1V8_GPIO;
		bn_lcd_data.lcd_cm_gpio		= 0;
		bn_lcd_data.lcd_dcr_gpio	= 0;

		memset(&bn_lcd_panel, 0, sizeof(bn_lcd_panel));

		bn_lcd_panel.name					= "auo_lcd";
		bn_lcd_panel.driver_name			= "auo";
		bn_lcd_panel.type					= OMAP_DISPLAY_TYPE_DPI;
		bn_lcd_panel.data					= &bn_lcd_data;
		bn_lcd_panel.phy.dpi.data_lines		= 24;
		bn_lcd_panel.channel				= OMAP_DSS_CHANNEL_LCD2;

		bn_lcd_panel.platform_enable		= NULL;
		bn_lcd_panel.platform_disable		= NULL;

		bn_lcd_panel.clocks.dispc.dispc_fclk_src	= OMAP_DSS_CLK_SRC_FCK;
		bn_lcd_panel.clocks.hdmi.regn		= 15;
		bn_lcd_panel.clocks.hdmi.regm2		= 1;
		bn_lcd_panel.clocks.hdmi.max_pixclk_khz		= 148500;
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
#ifdef CONFIG_OMAP4_DSS_HDMI
	bn_hdmi_mux_init();
#endif

#ifdef CONFIG_MACH_OMAP_HUMMINGBIRD
	hummingbird_lcd_mux_init();
#endif

	omapfb_set_platform_data(&bn_fb_pdata);

	omap_display_init(&bn_dss_data);

#ifdef CONFIG_MACH_OMAP_OVATION
	if (system_rev < OVATION_EVT1B)
		bus_i = 1;
#endif

	i2c_register_board_info(bus_i, lp8556_i2c_boardinfo,
						ARRAY_SIZE(lp8556_i2c_boardinfo));

	return 0;
}
