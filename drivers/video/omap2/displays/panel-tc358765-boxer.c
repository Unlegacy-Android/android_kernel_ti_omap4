/*
 * Toshiba TC358765 DSI-to-LVDS chip driver
 *
 * Copyright (C) Texas Instruments
 * Author: Tomi Valkeinen <tomi.valkeinen@ti.com>
 *
 * Based on original version from Jerry Alexander <x0135174@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define DEBUG

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>

#include <video/omapdss.h>
#include <video/omap-panel-tc358765.h>

#include "panel-tc358765.h"

#define BOXER_WIDTH	1024
#define BOXER_HEIGHT	600
#define BOXER_PCLK	46000
#if 0
# original from acclaim
#define BOXER_HFP	160
#define BOXER_HSW	10
#define BOXER_HBP	160
#else
/*  (hfp + hsw + hbp + xres) % 4[mipi data lane] must be zero */
#define BOXER_HFP	159
#define BOXER_HSW	10
#define BOXER_HBP	159
#endif
#define BOXER_VFP	10
#define BOXER_VSW	2
#define BOXER_VBP	23

#define BOXER_LCD_RST_DELAY		100
#define BOXER_LCD_INIT_DELAY		200
static struct spi_device *boxer_spi;

static struct omap_video_timings tc358765_timings = {
	.x_res		= BOXER_WIDTH,
	.y_res		= BOXER_HEIGHT,
	.pixel_clock	= BOXER_PCLK,
	.hfp		= BOXER_HFP,
	.hsw            = BOXER_HSW,
	.hbp            = BOXER_HBP,
	.vfp            = BOXER_VFP,
	.vsw            = BOXER_VSW,
	.vbp            = BOXER_VBP,
};

/* device private data structure */
struct tc358765_data {
	struct mutex lock;

	struct omap_dss_device *dssdev;

	int channel0;
	int channel1;
};

static int boxer_spi_send(struct spi_device *spi,
		    unsigned char reg_addr, unsigned char reg_data)
{
	uint16_t msg = 0;

	msg = (reg_addr << 10) | reg_data;

	if (spi_write(spi, (unsigned char *) &msg, 2))
		printk(KERN_ERR "error in spi_write %x\n", msg);

	udelay(10);

	return 0;
}

static struct tc358765_board_data *get_board_data(struct omap_dss_device *dssdev)
{
	return (struct tc358765_board_data *)dssdev->data;
}

static int tc358765_read_register(struct omap_dss_device *dssdev, u16 reg)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);
	u8 buf[4];
	u32 val;
	int r;

	r = dsi_vc_gen_read_2(dssdev, d2d->channel1, reg, buf, 4);
	if (r < 0) {
		dev_err(&dssdev->dev, "gen read failed\n");
		return r;
	}

	val = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
	dev_dbg(&dssdev->dev, "reg read %x, val=%08x\n", reg, val);
	return 0;
}

static int tc358765_write_register(struct omap_dss_device *dssdev, u16 reg,
		u32 value)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);
	u8 buf[6];
	int r;

	buf[0] = (reg >> 0) & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	buf[2] = (value >> 0) & 0xff;
	buf[3] = (value >> 8) & 0xff;
	buf[4] = (value >> 16) & 0xff;
	buf[5] = (value >> 24) & 0xff;

	r = dsi_vc_gen_write_nosync(dssdev, d2d->channel1, buf, 6);
	if (r)
		dev_err(&dssdev->dev, "reg write reg(%x) val(%x) failed: %d\n",
			       reg, value, r);
	return r;
}



static void tc358765_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void tc358765_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
}

static int tc358765_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	if (tc358765_timings.x_res != timings->x_res ||
			tc358765_timings.y_res != timings->y_res ||
			tc358765_timings.pixel_clock != timings->pixel_clock ||
			tc358765_timings.hsw != timings->hsw ||
			tc358765_timings.hfp != timings->hfp ||
			tc358765_timings.hbp != timings->hbp ||
			tc358765_timings.vsw != timings->vsw ||
			tc358765_timings.vfp != timings->vfp ||
			tc358765_timings.vbp != timings->vbp)
		return -EINVAL;

	return 0;
}

static void tc358765_get_resolution(struct omap_dss_device *dssdev,
		u16 *xres, u16 *yres)
{
	*xres = tc358765_timings.x_res;
	*yres = tc358765_timings.y_res;
}

static int tc358765_hw_reset(struct omap_dss_device *dssdev)
{
	struct tc358765_board_data *board_data = get_board_data(dssdev);

	if (board_data == NULL || board_data->reset_gpio == -1)
		return 0;

	gpio_set_value(board_data->reset_gpio, 1);
	udelay(100);
	/* reset the panel */
	gpio_set_value(board_data->reset_gpio, 0);
	/* assert reset */
	udelay(100);
	gpio_set_value(board_data->reset_gpio, 1);

	/* wait after releasing reset */
	msleep(100);

	return 0;
}

static int tc358765_probe(struct omap_dss_device *dssdev)
{
	struct tc358765_data *d2d;
	struct tc358765_board_data *board_data = get_board_data(dssdev);
	int r = 0;

	dev_dbg(&dssdev->dev, "tc358765_probe\n");

	tc358765_timings.x_res = board_data->x_res;
	tc358765_timings.y_res = board_data->y_res;
	dssdev->panel.config = OMAP_DSS_LCD_TFT;
	dssdev->panel.timings = tc358765_timings;
	dssdev->ctrl.pixel_size = 24;
	dssdev->panel.acbi = 0;
	dssdev->panel.acb = 40;

	d2d = kzalloc(sizeof(*d2d), GFP_KERNEL);
	if (!d2d) {
		r = -ENOMEM;
		return r;
	}

	d2d->dssdev = dssdev;

	mutex_init(&d2d->lock);

	dev_set_drvdata(&dssdev->dev, d2d);

	r = omap_dsi_request_vc(dssdev, &d2d->channel0);
	if (r)
		dev_err(&dssdev->dev, "failed to get virtual channel0\n");

	r = omap_dsi_set_vc_id(dssdev, d2d->channel0, 0);
	if (r)
		dev_err(&dssdev->dev, "failed to set VC_ID0\n");

	r = omap_dsi_request_vc(dssdev, &d2d->channel1);
	if (r)
		dev_err(&dssdev->dev, "failed to get virtual channel1\n");

	r = omap_dsi_set_vc_id(dssdev, d2d->channel1, 0);
	if (r)
		dev_err(&dssdev->dev, "failed to set VC_ID1\n");

	dev_dbg(&dssdev->dev, "tc358765_probe done\n");

	/* do I need an err and kfree(d2d) */
	return r;
}

static void tc358765_remove(struct omap_dss_device *dssdev)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);

	omap_dsi_release_vc(dssdev, d2d->channel0);
	omap_dsi_release_vc(dssdev, d2d->channel1);

	kfree(d2d);
}

static struct
{
	u16 reg;
	u32 data;
} tc358765_init_seq[] = {
	/* this register setting is required only if host wishes to
	 * perform DSI read transactions
	 */
	{ PPI_TX_RX_TA, 0x00000004 },
	/* SYSLPTX Timing Generation Counter */
	{ PPI_LPTXTIMECNT, 0x00000004 },
	/* D*S_CLRSIPOCOUNT = [(THS-SETTLE + THS-ZERO) / HS_byte_clock_period ] */
	{ PPI_D0S_CLRSIPOCOUNT, 0x00000003 },
	{ PPI_D1S_CLRSIPOCOUNT, 0x00000003 },
	{ PPI_D2S_CLRSIPOCOUNT, 0x00000003 },
	{ PPI_D3S_CLRSIPOCOUNT, 0x00000003 },
	/* SpeedLaneSel == HS4L */
	{ DSI_LANEENABLE, 0x0000001F },
	/* SpeedLaneSel == HS4L */
	{ PPI_LANEENABLE, 0x0000001F },
	/* Changed to 1 */
	{ PPI_STARTPPI, 0x00000001 },
	/* Changed to 1 */
	{ DSI_STARTDSI, 0x00000001 },

	/* configure D2L on-chip PLL */
	{ LVPHY1, 0x00000000 },
	/* set frequency range allowed and clock/data lanes */
	{ LVPHY0, 0x00044006 },

	/* REVISIT
	  Single In / Single Out
	  Rising Clock Edge
	  24 bit mode,
	  NOT using EXTCLK, using DSI D-phy clock lane as PCLK
	*/
	/* configure D2L chip LCD Controller configuration registers */
	{ VPCTRL, 0x00F00110 },
	{ HTIM1, ((BOXER_HBP << 16) | BOXER_HSW)},
	{ HTIM2, ((BOXER_HFP << 16) | BOXER_WIDTH)},
	{ VTIM1, ((BOXER_VBP << 16) | BOXER_VSW)},
	{ VTIM2, ((BOXER_VFP << 16) | BOXER_HEIGHT)},
	{ LVCFG, 0x00000001 },

	/* Issue a soft reset to LCD Controller for a clean start */
	{ SYSRST, 0x00000004 },
	{ VFUEN, 0x00000001 },
};

static int tc358765_write_init_config(struct omap_dss_device *dssdev)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);
	struct tc358765_board_data *board_data = get_board_data(dssdev);
	int i;
	int r;

	for (i = 0; i < ARRAY_SIZE(tc358765_init_seq); ++i) {
		u16 reg = tc358765_init_seq[i].reg;
		u32 data = tc358765_init_seq[i].data;

		r = tc358765_write_register(dssdev, reg, data);
		if (r) {
			dev_err(&dssdev->dev,
					"failed to write initial config (write) %d\n", i);
			return r;
		}

		/* send the first commands without bta */
		if (i < 3)
			continue;

		r = dsi_vc_send_bta_sync(dssdev, d2d->channel1);
		if (r) {
			dev_err(&dssdev->dev,
					"failed to write initial config (BTA) %d\n", i);
			return r;
		}
	}
	tc358765_write_register(dssdev, HTIM2,
		(BOXER_HFP << 16) | board_data->x_res);
	tc358765_write_register(dssdev, VTIM2,
		(BOXER_VFP << 16) | board_data->y_res);

	return 0;
}

static int tc358765_power_on(struct omap_dss_device *dssdev)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r;

	/* At power on the first vsync has not been received yet */
	dssdev->first_vsync = false;

	dev_dbg(&dssdev->dev, "power_on\n");

	if (dssdev->platform_enable)
		dssdev->platform_enable(dssdev);

	r = omapdss_dsi_display_enable(dssdev);
	if (r) {
		dev_err(&dssdev->dev, "failed to enable DSI\n");
		goto err_disp_enable;
	}

	omapdss_dsi_vc_enable_hs(dssdev, d2d->channel0, true);

	/* reset tc358765 bridge */
	tc358765_hw_reset(dssdev);

	/* do extra job to match kozio registers (???) */
	dsi_videomode_panel_preinit(dssdev);

	/* Need to wait a certain time - Toshiba Bridge Constraint */
	/* msleep(400); */

	/* configure D2L chip DSI-RX configuration registers */
	r = tc358765_write_init_config(dssdev);
	if (r)
		goto err_write_init;

	tc358765_read_register(dssdev, PPI_TX_RX_TA);

	dsi_vc_send_bta_sync(dssdev, d2d->channel1);
	dsi_vc_send_bta_sync(dssdev, d2d->channel1);
	dsi_vc_send_bta_sync(dssdev, d2d->channel1);

	tc358765_read_register(dssdev, PPI_TX_RX_TA);

	omapdss_dsi_vc_enable_hs(dssdev, d2d->channel1, true);

	/* 0x0e - 16bit
	 * 0x1e - packed 18bit
	 * 0x2e - unpacked 18bit
	 * 0x3e - 24bit
	 */
	dsi_video_mode_enable(dssdev, 0x3e);

	dev_dbg(&dssdev->dev, "power_on done\n");

	return r;

err_write_init:
	omapdss_dsi_display_disable(dssdev, false, false);
err_disp_enable:
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	return r;
}

static void tc358765_power_off(struct omap_dss_device *dssdev)
{
	dsi_video_mode_disable(dssdev, false);

	omapdss_dsi_display_disable(dssdev, false, false);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);
}

static void tc358765_disable(struct omap_dss_device *dssdev)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "disable\n");

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		mutex_lock(&d2d->lock);
		dsi_bus_lock(dssdev);

		tc358765_power_off(dssdev);

		dsi_bus_unlock(dssdev);
		mutex_unlock(&d2d->lock);
	}

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static void boxer_start(struct omap_dss_device *dssdev)
{
	printk(KERN_INFO " boxer : %s sending SPI data , line %d\n", __FUNCTION__ , __LINE__);
	msleep(BOXER_LCD_RST_DELAY);
	boxer_spi_send(boxer_spi, 0x00, 0xad);
	boxer_spi_send(boxer_spi, 0x01, 0x30);
	boxer_spi_send(boxer_spi, 0x02, 0x40);
	boxer_spi_send(boxer_spi, 0x0e, 0x5f);
	boxer_spi_send(boxer_spi, 0x0f, 0xa4);
	boxer_spi_send(boxer_spi, 0x0d, 0x00);
	boxer_spi_send(boxer_spi, 0x02, 0x43);
	boxer_spi_send(boxer_spi, 0x0a, 0x28);
	boxer_spi_send(boxer_spi, 0x10, 0x41);
	msleep(BOXER_LCD_INIT_DELAY);
}

static int tc358765_enable(struct omap_dss_device *dssdev)
{
	struct tc358765_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r = 0;

	dev_dbg(&dssdev->dev, "enable\n");

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)
		return -EINVAL;

	mutex_lock(&d2d->lock);
	dsi_bus_lock(dssdev);

	r = tc358765_power_on(dssdev);

	dsi_bus_unlock(dssdev);

	if (r) {
		dev_dbg(&dssdev->dev, "enable failed\n");
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	} else {
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	}

	mutex_unlock(&d2d->lock);

	boxer_start(dssdev);

	return r;
}

static int tc358765_suspend(struct omap_dss_device *dssdev)
{
	/* Disable the panel and return 0;*/
	tc358765_disable(dssdev);
	return 0;
}

static struct omap_dss_driver tc358765_driver = {
	.probe		= tc358765_probe,
	.remove		= tc358765_remove,

	.enable		= tc358765_enable,
	.disable	= tc358765_disable,
	.suspend	= tc358765_suspend,
	.resume		= NULL,

	.get_resolution	= tc358765_get_resolution,
	.get_recommended_bpp = omapdss_default_get_recommended_bpp,

	.get_timings	= tc358765_get_timings,
	.set_timings	= tc358765_set_timings,
	.check_timings	= tc358765_check_timings,

	.driver         = {
		.name   = "tc358765-boxer",
		.owner  = THIS_MODULE,
	},
};

static int boxer_spi_probe(struct spi_device *spi)
{
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 16;

	spi_setup(spi);

	boxer_spi = spi;

	return omap_dss_register_driver(&tc358765_driver);
}

static int boxer_spi_remove(struct spi_device *spi)
{
	omap_dss_unregister_driver(&tc358765_driver);
	return 0;
}

static struct spi_driver boxer_spi_driver = {
	.probe		= boxer_spi_probe,
	.remove		= __devexit_p(boxer_spi_remove),
	.driver		= {
		.name	= "boxer_disp_spi",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
};

static int __init tc358765_init(void)
{
	return spi_register_driver(&boxer_spi_driver);
}

static void __exit tc358765_exit(void)
{
	spi_unregister_driver(&boxer_spi_driver);
}

module_init(tc358765_init);
module_exit(tc358765_exit);

MODULE_AUTHOR("Tomi Valkeinen <tomi.valkeinen@ti.com>");
MODULE_DESCRIPTION("TC358765 DSI-2-LVDS Driver");
MODULE_LICENSE("GPL");
