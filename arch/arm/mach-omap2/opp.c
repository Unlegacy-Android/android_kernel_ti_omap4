/*
 * OMAP SoC specific OPP wrapper function
 *
 * Copyright (C) 2009-2010 Texas Instruments Incorporated - http://www.ti.com/
 *	Nishanth Menon
 *	Kevin Hilman
 * Copyright (C) 2010 Nokia Corporation.
 *      Eduardo Valentin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/opp.h>
#include <linux/clk.h>
#include <linux/rculist.h>
#include <linux/slab.h>

#include <plat/omap_device.h>
#include <plat/clock.h>

#include "omap_opp_data.h"
#include "dvfs.h"

extern struct sysfs_dirent *sysfs_find_dirent(struct sysfs_dirent *parent_sd,
					const void *ns,
					const unsigned char *name);

/* Temp variable to allow multiple calls */
static u8 __initdata omap_table_init;

static LIST_HEAD(dev_opp_list);

struct opp {
	struct list_head node;

	bool available;
	unsigned long rate;
	unsigned long u_volt;

	struct device_opp *dev_opp;
};

struct device_opp {
	struct list_head node;

	struct device *dev;
	struct list_head opp_list;
};

static struct device_opp *find_device_opp(struct device *dev)
{
	struct device_opp *dev_opp;

	if (unlikely(IS_ERR_OR_NULL(dev))) {
		return ERR_PTR(-EINVAL);
	}

	list_for_each_entry_rcu(dev_opp, &dev_opp_list, node) {
		if (dev_opp->dev == dev) {
			return dev_opp;
		}
	}

	return ERR_PTR(-ENODEV);
}

static int store_opp(struct device *dev, struct omap_opp_def *opp_def) {
	struct device_opp *dev_opp = NULL;
	struct opp *opp, *new_opp;
	struct list_head *head;

	new_opp = kzalloc(sizeof(struct opp), GFP_KERNEL);
	if (!new_opp) {
		return -ENOMEM;
	}

	dev_opp = find_device_opp(dev);
	if (IS_ERR(dev_opp)) {
		dev_opp = kzalloc(sizeof(struct device_opp), GFP_KERNEL);
		if (!dev_opp) {
			kfree(new_opp);
			return -ENOMEM;
		}

		dev_opp->dev = dev;
		INIT_LIST_HEAD(&dev_opp->opp_list);

		list_add_rcu(&dev_opp->node, &dev_opp_list);
	}

	new_opp->dev_opp = dev_opp;
	new_opp->rate = opp_def->freq;
	new_opp->u_volt = opp_def->u_volt;
	new_opp->available = opp_def->default_available;

	head = &dev_opp->opp_list;
	list_for_each_entry_rcu(opp, &dev_opp->opp_list, node) {
		if (new_opp->rate < opp->rate)
			break;
		else
			head = &opp->node;
	}

	list_add_rcu(&new_opp->node, head);

	return 0;
}

static ssize_t show_avail_opp(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct device_opp *dev_opp;
	struct opp *opp_node;
	ssize_t count = 0;

	dev_opp = find_device_opp(dev);

	if (IS_ERR(dev_opp))
		return -EINVAL;

	count += snprintf(&buf[count], PAGE_SIZE,
				"\n|    Rate [Hz]    |"
				"   Voltage [uV]   | Default Available |\n\n");

	list_for_each_entry_rcu(opp_node, &dev_opp->opp_list, node) {
		count += snprintf(&buf[count], PAGE_SIZE,
					" %12lu   %14lu   %14s\n",
					opp_node->rate, opp_node->u_volt,
					opp_node->available ? "Yes" : "No");
	}

	return count;
}

static ssize_t show_current_opp(struct device *dev,
				struct device_attribute *attr, char *buf) {

	struct clk *devclk = dvfs_get_dev_clk(dev);
	if(!devclk)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%lu\n", clk_get_rate(devclk));
}

static ssize_t store_current_opp(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count) {
	unsigned long freq;
	int ret;

	ret = sscanf(buf, "%lu", &freq);
	if (ret != 1)
		return -EINVAL;

	printk(KERN_INFO "Scaling device freq to %lu [%d]\n", freq, ret);

	ret = omap_device_scale(dev, dev, freq);
	if(ret) {
		printk("Failed to scale device freq to %lu [%d]\n", freq, ret);
		return ret;
	}

	return count;
}

static struct device_attribute opp_attributes[] = {

	__ATTR(available_opp_list, S_IRUGO, show_avail_opp, NULL),
	__ATTR(current_opp, S_IRUGO | S_IWUSR, show_current_opp,
			store_current_opp),
};

static int opp_create_sysfs(struct omap_opp_def *opp_def)
{
	int i;
	int r;
	struct omap_hwmod *oh = NULL;
	struct device *dev = NULL;

	oh = omap_hwmod_lookup(opp_def->hwmod_name);
	if (oh && oh->od)
		dev = &oh->od->pdev.dev;

	for (i = 0; i < ARRAY_SIZE(opp_attributes); ++i) {
		if(sysfs_find_dirent(dev->kobj.sd, NULL,
					opp_attributes[i].attr.name))
			continue;

		r = device_create_file(dev, &opp_attributes[i]);
		if (r)
			return r;
	}

	return 0;
}

/**
 * omap_init_opp_table() - Initialize opp table as per the CPU type
 * @opp_def:		opp default list for this silicon
 * @opp_def_size:	number of opp entries for this silicon
 *
 * Register the initial OPP table with the OPP library based on the CPU
 * type. This is meant to be used only by SoC specific registration.
 */
int __init omap_init_opp_table(struct omap_opp_def *opp_def,
		u32 opp_def_size)
{
	int i, r;
	struct clk *clk;
	long round_rate;

	if (!opp_def || !opp_def_size) {
		pr_err("%s: invalid params!\n", __func__);
		return -EINVAL;
	}

	/*
	 * Initialize only if not already initialized even if the previous
	 * call failed, because, no reason we'd succeed again.
	 */
	if (omap_table_init)
		return -EEXIST;
	omap_table_init = 1;

	/* Lets now register with OPP library */
	for (i = 0; i < opp_def_size; i++, opp_def++) {
		struct omap_hwmod *oh;
		struct device *dev;

		if (!opp_def->hwmod_name) {
			WARN(1, "%s: NULL name of omap_hwmod, failing"
				" [%d].\n", __func__, i);
			continue;
		}
		oh = omap_hwmod_lookup(opp_def->hwmod_name);
		if (!oh || !oh->od) {
			pr_warn("%s: no hwmod or odev for %s, [%d] "
				"cannot add OPPs.\n", __func__,
				opp_def->hwmod_name, i);
			continue;
		}
		dev = &oh->od->pdev.dev;

		clk = omap_clk_get_by_name(opp_def->clk_name);
		if (clk) {
			round_rate = clk_round_rate(clk, opp_def->freq);
			if (round_rate > 0) {
				opp_def->freq = round_rate;
			} else {
				WARN(1, "%s: round_rate for clock %s failed\n",
					__func__, opp_def->clk_name);
				continue; /* skip Bad OPP */
			}
		} else {
			WARN(1, "%s: No clock by name %s found\n", __func__,
				opp_def->clk_name);
			continue; /* skip Bad OPP */
		}
		r = opp_add(dev, opp_def->freq, opp_def->u_volt);
		if (r) {
			dev_err(dev, "%s: add OPP %ld failed for %s [%d] "
				"result=%d\n",
			       __func__, opp_def->freq,
			       opp_def->hwmod_name, i, r);
		} else {
			if (!opp_def->default_available)
				r = opp_disable(dev, opp_def->freq);
			if (r)
				dev_err(dev, "%s: disable %ld failed for %s "
					"[%d] result=%d\n",
					__func__, opp_def->freq,
					opp_def->hwmod_name, i, r);

			r  = omap_dvfs_register_device(dev,
				opp_def->voltdm_name, opp_def->clk_name);
			if (r)
				dev_err(dev, "%s:%s:err dvfs register %d %d\n",
					__func__, opp_def->hwmod_name, r, i);

			r = store_opp(dev, opp_def);
			if (r)
				dev_err(dev, "Failed to store opp\n");

			r = opp_create_sysfs(opp_def);
			if (r)
				dev_err(dev, "Failed to create sysfs entry\n");
		}
	}

	return 0;
}
