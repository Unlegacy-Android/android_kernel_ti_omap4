/*
 * Novatek TCON based display panel driver.
 *
 * Copyright (C) Barnes & Noble Inc. 2012
 *
 * Based on original version from:
 *	Jerry Alexander <x0135174@ti.com>
 *	Tomi Valkeinen <tomi.valkeinen@ti.com>
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
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/i2c/maxim9606.h>
#include <linux/earlysuspend.h>

#include <video/omapdss.h>
#include <video/omap-panel-dsi.h>

#include "../dss/dss.h"

static unsigned char first_suspend = 0;

/* device private data structure */
struct novatek_data {
	struct mutex lock;

	struct omap_dss_device *dssdev;

	int channel0;
	int channel_cmd;
};

struct maxim9606 {
	struct i2c_client *client;
	struct maxim9606_platform_data *pdata;
	struct early_suspend suspend;
	bool softstart;
};

#define I2C_WRITE_MSG(a, d) { .addr = a, .flags = 0, .len = sizeof(d), .buf = d }

static void maxim9606_enable(struct maxim9606 *mx)
{
	struct timespec boost_on;
	struct timespec boost_done;

	int retry_count = 0;
	u8 value = 0;
	s32 r = 0;

	u8 test_mode[3] = { 0xFF, 0x54, 0x4D };
	u8 cmd1[2] = { 0x5F, 0x02 };
	u8 cmd2[2] = { 0x60, 0x14 };
	u8 cmd3[2] = { 0x10, 0xDF };
	u8 cmd4[2] = { 0x5F, 0x00 };
	u8 cmd5[2] = { 0xFF, 0x00 };

	struct i2c_msg seq[6] = {
		I2C_WRITE_MSG(0x74, test_mode),
		I2C_WRITE_MSG(0x74, cmd1),
		I2C_WRITE_MSG(0x74, cmd2),
		I2C_WRITE_MSG(0x74, cmd3),
		I2C_WRITE_MSG(0x74, cmd4),
		I2C_WRITE_MSG(0x74, cmd5),
	};

retry:
	if (retry_count > 5) {
		dev_err(&mx->client->dev, "retry count exceeded, bailing\n");
		return;
	}

	retry_count++;
	if (mx->pdata->power_on) {
		mx->pdata->power_on(&mx->client->dev);
	}

	if (!mx->softstart) {
		dev_info(&mx->client->dev, "soft start disabled");
		return;
	}

	msleep(20);

	ktime_get_ts(&boost_on);
	r = i2c_smbus_write_byte_data(mx->client, 0x10, 0xD7);
	udelay(1000);

	if (r < 0) {
		dev_warn(&mx->client->dev, "disable boost %d\n", r);

		if (mx->pdata->power_off) {
			mx->pdata->power_off(&mx->client->dev);
		}

		goto retry;
	}

	r = i2c_transfer(mx->client->adapter, seq, ARRAY_SIZE(seq));
	ktime_get_ts(&boost_done);

	if (r < 0) {
		dev_warn(&mx->client->dev, "soft start %d\n", r);
	}

	msleep(55);
	value = i2c_smbus_read_byte_data(mx->client, 0x39);
	
	if (r < 0 || value != 0) {
		dev_warn(&mx->client->dev, "maxim status %hhu %d\n", value, r);
	}

	if (ktime_us_delta(timespec_to_ktime(boost_done), timespec_to_ktime(boost_on)) > 3000) {
		dev_warn(&mx->client->dev, "boost on took too long, %lld us\n", ktime_us_delta(timespec_to_ktime(boost_done), timespec_to_ktime(boost_on)));

		if (mx->pdata->power_off) {
			mx->pdata->power_off(&mx->client->dev);
		}

		goto retry;
	}
}

static void maxim9606_disable(struct maxim9606 *mx)
{
	if (mx->pdata->power_off) {
		mx->pdata->power_off(&mx->client->dev);
	}
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void maxim9606_late_resume(struct early_suspend *h)
{
	struct maxim9606 *mx = container_of(h, struct maxim9606, suspend);
	maxim9606_enable(mx);
}

static void maxim9606_early_suspend(struct early_suspend *h)
{
	struct maxim9606 *mx = container_of(h, struct maxim9606, suspend);
	maxim9606_disable(mx);
}
#endif

#define DELAY2_VALUE (63)
#define DELAY3_VALUE (15)
#define DELAY4_VALUE (15)

static int maxim9606_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct maxim9606_platform_data *pdata = cl->dev.platform_data;
	struct maxim9606 *mx;
	s32 DELAY2, DELAY3, DELAY4;
	
	if (pdata->request_resources) {
		pdata->request_resources(&cl->dev);
	}	

	mx = kzalloc(sizeof(struct maxim9606), GFP_KERNEL);

	if (!mx) 
		goto err_mem;

	mx->client = cl;
	mx->pdata = pdata;
	mx->softstart = true;

	i2c_set_clientdata(cl, mx);

#ifdef CONFIG_HAS_EARLYSUSPEND
	// make sure this runs BEFORE the panel
	mx->suspend.level 	= EARLY_SUSPEND_LEVEL_DISABLE_FB + 2;
	mx->suspend.suspend = maxim9606_early_suspend;
	mx->suspend.resume	= maxim9606_late_resume;
	register_early_suspend(&mx->suspend);
#endif

	DELAY2 = i2c_smbus_read_byte_data(mx->client, 0x04);
	DELAY3 = i2c_smbus_read_byte_data(mx->client, 0x05);
	DELAY4 = i2c_smbus_read_byte_data(mx->client, 0x06);

	if ((DELAY2 != DELAY2_VALUE) || 
		(DELAY3 != DELAY3_VALUE) ||
		(DELAY4 != DELAY4_VALUE)) {
		dev_warn(&cl->dev, "DELAYs not programmed correctly, 2 %d 3 %d 4 %d, soft start disabled\n", DELAY2, DELAY3, DELAY4);
		mx->softstart = false;
	} 

	if (i2c_smbus_read_byte_data(mx->client, 0x39)) {
		maxim9606_enable(mx);
	} else if (mx->pdata->power_on) {
		mx->pdata->power_on(&mx->client->dev);
	}

	return 0;

err_mem:
	return -ENOMEM;
}

static int maxim9606_remove(struct i2c_client *cl)
{
	struct maxim9606 *mx = i2c_get_clientdata(cl);
	
	maxim9606_disable(mx);
	unregister_early_suspend(&mx->suspend);

	if (mx->pdata->release_resources) {
		mx->pdata->release_resources(&cl->dev);
	}	

	kfree(mx);

	return 0;
}

static void maxim9606_shutdown(struct i2c_client *cl)
{
	struct maxim9606 *mx = i2c_get_clientdata(cl);

	maxim9606_disable(mx);
}

#define PMIC_DRIVER		"maxim9606"

static const struct i2c_device_id maxim9606_i2c_id[] = {
	{ PMIC_DRIVER, 0x74 },
	{ },
};

static struct i2c_driver maxim9606_driver = {
	.driver		= {
		.name	= PMIC_DRIVER,
		.owner = THIS_MODULE,
	},

	.probe		= maxim9606_probe,
	.remove		= maxim9606_remove,
	.shutdown	= maxim9606_shutdown,
	.id_table	= maxim9606_i2c_id,
};

static void novatek_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static void novatek_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
}

static int novatek_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	if (dssdev->panel.timings.x_res != timings->x_res ||
		dssdev->panel.timings.y_res != timings->y_res ||
		dssdev->panel.timings.pixel_clock != timings->pixel_clock ||
		dssdev->panel.timings.hsw != timings->hsw ||
		dssdev->panel.timings.hfp != timings->hfp ||
		dssdev->panel.timings.hbp != timings->hbp ||
		dssdev->panel.timings.vsw != timings->vsw ||
		dssdev->panel.timings.vfp != timings->vfp ||
		dssdev->panel.timings.vbp != timings->vbp)
		return -EINVAL;

	return 0;
}

static void novatek_get_resolution(struct omap_dss_device *dssdev,
		u16 *xres, u16 *yres)
{
	*xres = dssdev->panel.timings.x_res;
	*yres = dssdev->panel.timings.y_res;
}

static int novatek_probe(struct omap_dss_device *dssdev)
{
	struct novatek_data *d2d;
	int r = 0;

	dev_dbg(&dssdev->dev, "novatek_probe\n");

	dssdev->panel.config = OMAP_DSS_LCD_TFT;
	dssdev->panel.dsi_pix_fmt = bpp_to_datatype(dssdev->ctrl.pixel_size);

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

	r = omap_dsi_request_vc(dssdev, &d2d->channel_cmd);
	if (r)
		dev_err(&dssdev->dev, "failed to get virtual channel_cmd\n");

	r = omap_dsi_set_vc_id(dssdev, d2d->channel_cmd, 0);
	if (r)
		dev_err(&dssdev->dev, "failed to set VC_ID1\n");

	dev_dbg(&dssdev->dev, "novatek_probe done\n");

	/* do I need an err and kfree(d2d) */
	return r;
}

static void novatek_remove(struct omap_dss_device *dssdev)
{
	struct novatek_data *d2d = dev_get_drvdata(&dssdev->dev);

	omap_dsi_release_vc(dssdev, d2d->channel0);
	omap_dsi_release_vc(dssdev, d2d->channel_cmd);

	kfree(d2d);
}

static int novatek_power_on(struct omap_dss_device *dssdev)
{
	struct novatek_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r;

	/* At power on the first vsync has not been received yet */
	dssdev->first_vsync = false;

	dev_dbg(&dssdev->dev, "power_on -- skip_init==%d\n", dssdev->skip_init);

	if (dssdev->platform_enable)
		dssdev->platform_enable(dssdev);

	r = omapdss_dsi_display_enable(dssdev);	
	if (r) {
		dev_err(&dssdev->dev, "failed to enable DSI\n");
		goto err_disp_enable;
	}
	
	/* first_suspend flag is to ignore suspend one time during init process
	we use this flag to re-init the TCON once by disable/enable panel
	voltage. From the next time suspend will help doing this */

	if (!dssdev->skip_init) {
		msleep(1);
		r = dsi_vc_turn_on_peripheral(dssdev, d2d->channel0);
		msleep(1);

		if (r) {
			dev_err(&dssdev->dev, "turn on peripheral failed: %d", r);
		}

		/* do extra job to match kozio registers (???) */
		dsi_videomode_panel_preinit(dssdev);
		msleep(1);

		/* Go to HS mode after sending all the DSI commands in
		 * LP mode
		 */
		omapdss_dsi_vc_enable_hs(dssdev, d2d->channel0, true);
		omapdss_dsi_vc_enable_hs(dssdev, d2d->channel_cmd, true);

		dsi_enable_video_output(dssdev, d2d->channel0);
	} else {
		r = dss_mgr_enable(dssdev->manager);
		dssdev->skip_init = false;
	}

	dev_dbg(&dssdev->dev, "power_on done\n");

	return r;

err_disp_enable:
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	return r;
}

static void novatek_power_off(struct omap_dss_device *dssdev)
{
	struct novatek_data *d2d = dev_get_drvdata(&dssdev->dev);
	dsi_disable_video_output(dssdev, d2d->channel0);
#if 0
	/* Send sleep in command to the TCON */
	dsi_vc_dcs_write_1(dssdev, 1, 0x11, 0x01);
#endif
	omapdss_dsi_display_disable(dssdev, false, false);

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);
#if 0
	if (novatek_lcd_disable(dssdev))
		dev_err(&dssdev->dev, "failed to disable LCD\n");
#endif
}

static void novatek_disable(struct omap_dss_device *dssdev)
{
	struct novatek_data *d2d = dev_get_drvdata(&dssdev->dev);

	dev_dbg(&dssdev->dev, "disable\n");

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		mutex_lock(&d2d->lock);
		dsi_bus_lock(dssdev);

		novatek_power_off(dssdev);

		dsi_bus_unlock(dssdev);
		mutex_unlock(&d2d->lock);
	}

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static int novatek_enable(struct omap_dss_device *dssdev)
{
	struct novatek_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r = 0;

	dev_dbg(&dssdev->dev, "enable\n");

	if (dssdev->state != OMAP_DSS_DISPLAY_DISABLED)
		return -EINVAL;

	mutex_lock(&d2d->lock);
	dsi_bus_lock(dssdev);

	r = novatek_power_on(dssdev);

	dsi_bus_unlock(dssdev);

	if (r) {
		dev_dbg(&dssdev->dev, "enable failed\n");
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	} else {
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	}

	mutex_unlock(&d2d->lock);

	return r;
}

static int novatek_suspend(struct omap_dss_device *dssdev)
{
	struct novatek_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r;

	dev_dbg(&dssdev->dev, "suspend\n");

	mutex_lock(&d2d->lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE) {
		r = -EINVAL;
		goto err;
	}

	dsi_bus_lock(dssdev);

	novatek_power_off(dssdev);

	dsi_bus_unlock(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

	mutex_unlock(&d2d->lock);

return 0;
err:
	mutex_unlock(&d2d->lock);
	return r;
}

static int novatek_resume(struct omap_dss_device *dssdev)
{
	struct novatek_data *d2d = dev_get_drvdata(&dssdev->dev);
	int r;

	if (first_suspend) {
		dev_dbg(&dssdev->dev, "not resuming now\n");
		first_suspend = 0;
		return 0;
	}
	dev_dbg(&dssdev->dev, "resume\n");

	mutex_lock(&d2d->lock);

	if (dssdev->state != OMAP_DSS_DISPLAY_SUSPENDED) {
		r = -EINVAL;
		goto err;
	}

	dsi_bus_lock(dssdev);

	r = novatek_power_on(dssdev);

	dsi_bus_unlock(dssdev);

	if (r)
		dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
	else
		dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	mutex_unlock(&d2d->lock);

	return r;
err:
	mutex_unlock(&d2d->lock);
	return r;
}

static struct omap_dss_driver novatek_driver = {
	.probe		= novatek_probe,
	.remove		= novatek_remove,

	.enable		= novatek_enable,
	.disable	= novatek_disable,
	.suspend	= novatek_suspend,
	.resume		= novatek_resume,

	.get_resolution	= novatek_get_resolution,
	.get_recommended_bpp = omapdss_default_get_recommended_bpp,

	.get_timings	= novatek_get_timings,
	.set_timings	= novatek_set_timings,
	.check_timings	= novatek_check_timings,

	.driver         = {
		.name   = "novatek-panel",
		.owner  = THIS_MODULE,
	},
};

static int __init novatek_init(void)
{
	int r;

	if (i2c_add_driver(&maxim9606_driver) < 0) {
		pr_err("Failed to register maxim9606 driver\n"); 
	}

	r = omap_dss_register_driver(&novatek_driver);
	if (r < 0) {
		pr_err("Failed to register novatek driver\n");
		return r;
	}

	return 0;
}

static void __exit novatek_exit(void)
{
	omap_dss_unregister_driver(&novatek_driver);
	i2c_del_driver(&maxim9606_driver);
}

module_init(novatek_init);
module_exit(novatek_exit);

MODULE_AUTHOR("David Bolcsfoldi <dbolcsfoldi@intrinsyc.com>");
MODULE_DESCRIPTION("Novatek TCON display");
MODULE_LICENSE("GPL");
