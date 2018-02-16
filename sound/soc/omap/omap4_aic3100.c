/*
 * omap4_aic3100.c - SoC audio for acclaim board
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/wakelock.h>

#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <asm/mach-types.h>
#include <mach/gpio.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/jack.h>
#include <sound/soc-dsp.h>
#include <sound/pcm_params.h>
#include <asm/mach-types.h>
#include <plat/hardware.h>
#include <plat/mcbsp.h>
#include <plat/mux.h>
#include "abe/abe_main.h"
#include "omap-abe.h"
#include "omap-dmic.h"
#include "omap-pcm.h"
#include "omap-mcbsp.h"

#include "../codecs/tlv320aic3100.h"

#if defined(CONFIG_SND_OMAP_SOC_ACCLAIM_NO_ABE) &&	\
	defined(CONFIG_SND_OMAP_SOC_ACCLAIM)
#error "Cannot compile with and without ABE support"
#endif

#define ACCLAIM_HS_DETECT_GPIO	102
#define CODEC_CLOCK 19200000
static struct clk *tlv320aic31xx_mclk;

static struct wake_lock omap_wakelock;
static int acclaim_headset_jack_status_check(void);
static struct platform_device *acclaim_snd_device;
static struct snd_soc_jack hs_jack;


/*
 * acclaim_evt_hw_params - Machine Driver's hw_params call-back handler routine.
 */
static int acclaim_evt_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	void __iomem *phymux_base = NULL;
	u32 phy_val;
	int ret;
	u32 curRate;

	DBG(KERN_INFO "%s: Entered\n", __func__);

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai,
				  SND_SOC_DAIFMT_I2S |
				  SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		printk(KERN_ERR "can't set codec DAI configuration\n");
		return ret;
	}

	/* Set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai,
				  SND_SOC_DAIFMT_I2S |
				  SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		printk(KERN_ERR "can't set cpu DAI configuration\n");
		return ret;
	}

	/* Enabling the 19.2 Mhz Master Clock Output
	from OMAP4 for Acclaim Board */
	phymux_base = ioremap(0x4A30A000, 0x1000);
	phy_val = __raw_readl(phymux_base + 0x0314);
	phy_val = (phy_val & 0xFFF0FEFF) | (0x00010100);
	__raw_writel(phy_val, phymux_base + 0x0314);
	/*iounmap(phymux_base);*/

	/* Added the test code to configure the McBSP4 CONTROL_MCBSP_LP
	 * register. This register ensures that the FSX and FSR on McBSP4 are
	 * internally short and both of them see the same signal from the
	 * External Audio Codec.
	 */
	phymux_base = ioremap(0x4a100000, 0x1000);
	__raw_writel(0xC0000000, phymux_base + 0x61c);

	/* Set the codec system clock for DAC and ADC. The
	 * third argument is specific to the board being used.
	 */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, 19200000,
				     SND_SOC_CLOCK_IN);
	if (ret < 0) {
		printk(KERN_ERR "can't set codec system clock\n");
		return ret;
	}

	DBG(KERN_INFO "omap4_hw_params passed...\n");

	return 0;
}

/*
 * @struct omap4_ops
 *
 * Structure for the Machine Driver Operations
 */
static struct snd_soc_ops acclaim_ops = {
	.hw_params = acclaim_evt_hw_params,
};

/* @struct hs_jack_pins
 *
 * Headset jack detection DAPM pins
 *
 * @pin:    name of the pin to update
 * @mask:   bits to check for in reported jack status
 * @invert: if non-zero then pin is enabled when status is not reported
 */
static struct snd_soc_jack_pin hs_jack_pins[] = {
	{
		.pin = "Onboard Mic",
		.mask = SND_JACK_MICROPHONE,
	},
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
};

/*
 * @struct hs_jack_gpios
 *
 * Headset jack detection gpios
 *
 * @gpio:         Pin 49 on the Qoo Rev 1 Boardmcbsp_be_hw_params_fixup
 * @name:         String Name "hsdet-gpio"
 * @report:       value to report when jack detected
 * @invert:       report presence in low state
 * @debouce_time: debouce time in ms
 */
static struct snd_soc_jack_gpio hs_jack_gpios[] = {
	{
		.gpio = ACCLAIM_HS_DETECT_GPIO,
		.name = "hsdet-gpio",
		.report = SND_JACK_HEADSET,
		.debounce_time = 200,
		.jack_status_check = acclaim_headset_jack_status_check,
	},
};


/* OMAP4 machine DAPM */
static const struct snd_soc_dapm_widget omap4_aic31xx_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Speaker Jack", NULL),
	SND_SOC_DAPM_MIC("Onboard Mic", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* External Speakers */
	{"Speaker Jack", NULL, "SPL"},

#ifdef AIC3110_CODEC_SUPPORT
	{"Speaker Jack", NULL, "SPR"},

	/* Onboard Mic: HSMIC with bias */
	{"MIC1LP", NULL, "Onboard Mic"},
	{"MIC1LM", NULL, "Onboard Mic"},
	{"MIC1RP", NULL, "Onboard Mic"},
#endif
	/* Headset Stereophone (Headphone): HSOL, HSOR */
	{"Headphone Jack", NULL, "HPL"},
	{"Headphone Jack", NULL, "HPR"},
};

/*
 * Initialization routine.
 */
static int acclaim_aic31xx_init(struct snd_soc_pcm_runtime *pcm)
{
	struct snd_soc_codec *codec = pcm->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int gpiostatus, ret;

	DBG(KERN_INFO  "acclaim_aic31xx_init..\n");

	ret = snd_soc_dapm_new_controls(dapm, omap4_aic31xx_dapm_widgets,
					ARRAY_SIZE(omap4_aic31xx_dapm_widgets));
	if (ret) {
		printk(KERN_INFO "snd_soc_dapm_new_controls failed.\n");
		return ret;
	}
	DBG("snd_soc_dapm_new_controls passed..\n");

	/* Set up OMAP4 specific audio path audio_map */
	ret = snd_soc_dapm_add_routes(dapm, audio_map,
					ARRAY_SIZE(audio_map));

	if (ret != 0)
		printk(KERN_INFO "snd_soc_dapm_add_routes failed..%d\n", ret);

	ret = snd_soc_dapm_sync(dapm);
	if (ret != 0) {
		printk(KERN_INFO "snd_soc_dapm_sync failed... %d\n", ret);
		return ret;
	}

	printk(KERN_INFO "snd_soc_dapm_sync passed..\n");

	gpiostatus = snd_soc_jack_new(codec, "Headset Jack",
				      SND_JACK_HEADSET, &hs_jack);
	if (gpiostatus != 0) {
		printk(KERN_ERR "snd_soc_jack_new failed(%d)\n", gpiostatus);
		return gpiostatus;
	}


	gpiostatus = snd_soc_jack_add_pins(&hs_jack, ARRAY_SIZE(hs_jack_pins),
					   hs_jack_pins);
	if (gpiostatus != 0) {
		printk(KERN_ERR"snd_soc_jack_add_pins failed(%d)\n",
			gpiostatus);
		return gpiostatus;
	}

	gpiostatus = snd_soc_jack_add_gpios(&hs_jack, ARRAY_SIZE(hs_jack_gpios),
					    hs_jack_gpios);

	if (gpiostatus != 0)
		printk(KERN_ERR "snd_soc_jack_add_gpios failed..%d\n",
			gpiostatus);

	/*
	 * The Codec Driver itself manages the POP
	 * polling and hence
	 * we will reset the ALSA pmdown_time to zero.
	 */
	pcm->pmdown_time = 0;
	snd_soc_dapm_enable_pin(dapm, "Onboard Mic");
	return 0;
}

/*
 * This function is to check the Headset Jack Status
 */
static int acclaim_headset_jack_status_check(void)
{
	int gpio_status, status;
	struct snd_soc_codec *codec = hs_jack.codec;
	struct aic31xx_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	printk(KERN_INFO "#Entered %s\n", __func__);
	gpio_status = gpio_get_value(ACCLAIM_HS_DETECT_GPIO);
	status = !(gpio_status);
	switch_set_state(&priv->hs_jack.sdev, status);
	if (hs_jack.codec != NULL) {
		if (!gpio_status) {
			printk(KERN_INFO "headset connected\n");
			snd_soc_dapm_disable_pin(dapm, "Speaker Jack");
			snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
			snd_soc_dapm_sync(dapm);
		} else {
			printk(KERN_INFO "headset not connected\n");
			snd_soc_dapm_enable_pin(dapm, "Speaker Jack");
			snd_soc_dapm_disable_pin(dapm, "Headphone Jack");
			snd_soc_dapm_sync(dapm);
		}
	}
	return 0;
}
static int mcbsp_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				     struct snd_pcm_hw_params *params)
{
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int be_id;
	unsigned int threshold;
	unsigned int val, min_mask;
	DBG("%s: Entered\n", __func__);
	DBG("%s: CPU DAI %s BE_ID %d\n", __func__, cpu_dai->name, \
						rtd->dai_link->be_id);

	be_id = rtd->dai_link->be_id;

	switch (be_id) {
	case OMAP_ABE_DAI_MM_FM:
		channels->min = 2;
		threshold = 2;
		val = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case OMAP_ABE_DAI_BT_VX:
		channels->min = 1;
		threshold = 1;
		val = SNDRV_PCM_FORMAT_S16_LE;
		break;
	default:
		threshold = 1;
		val = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}

	min_mask = snd_mask_min(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
				SNDRV_PCM_HW_PARAM_FIRST_MASK]);


	snd_mask_reset(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
			SNDRV_PCM_HW_PARAM_FIRST_MASK],
			min_mask);

	snd_mask_set(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
			SNDRV_PCM_HW_PARAM_FIRST_MASK], val);

	omap_mcbsp_set_tx_threshold(cpu_dai->id, threshold);
	omap_mcbsp_set_rx_threshold(cpu_dai->id, threshold);
	return 0;
}

static int aic31xx_stream_event(struct snd_soc_dapm_context *dapm)
{
	/*
	 * set DL1 gains dynamically according to the active output
	 * (Headset, Earpiece) and HSDAC power mode
	 */
	return 0;
}

struct snd_soc_dsp_link fe_MM = {
	.playback	= true,
	.capture	= true,
	.trigger = {SND_SOC_DSP_TRIGGER_BESPOKE, SND_SOC_DSP_TRIGGER_BESPOKE},
};

struct snd_soc_dsp_link fe_lp_MM = {
	.playback	= true,
	.trigger = {SND_SOC_DSP_TRIGGER_BESPOKE, SND_SOC_DSP_TRIGGER_BESPOKE},
};

struct snd_soc_dsp_link fe_MM_capture = {
	.capture	= true,
	.trigger = {SND_SOC_DSP_TRIGGER_BESPOKE, SND_SOC_DSP_TRIGGER_BESPOKE},
};

struct snd_soc_dsp_link fe_tones = {
	.playback	= true,
	.trigger = {SND_SOC_DSP_TRIGGER_BESPOKE, SND_SOC_DSP_TRIGGER_BESPOKE},
};

/* DAI_LINK Structure definition with both Front-End and
 * Back-end DAI Declarations.
 */
static struct snd_soc_dai_link acclaim_dai_link_abe[] = {

/*
 * Frontend DAIs - i.e. userspace visible interfaces (ALSA PCMs)
 */
	{
		.name = "tlv320aic3100 LP",
		.stream_name = "Multimedia",

		/* ABE components - MM-DL (mmap) */
		.cpu_dai_name = "MultiMedia1 LP",
		.platform_name = "aess",

		.dynamic = 1, /* BE is dynamic */
		.dsp_link = &fe_lp_MM
	},

	{
		.name = "tlv320aic3100",
		.stream_name = "Multimedia",

		/* ABE components - MM-UL & MM_DL */
		.cpu_dai_name = "MultiMedia1",
		.platform_name = "omap-pcm-audio",

		.dynamic = 1, /* BE is dynamic */
		.dsp_link = &fe_MM
	},
	{
		.name = "tlv320aic3100 capture",
		.stream_name = "Multimedia Capture",

		/* ABE components - MM-UL2 */
		.cpu_dai_name = "MultiMedia2",
		.platform_name = "omap-pcm-audio",

		.dynamic = 1, /* BE is dynamic */
		.dsp_link = &fe_MM_capture,
	},
	{
		.name = "Legacy McBSP",
		.stream_name = "Multimedia",

		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp-dai.1",
		.platform_name = "omap-pcm-audio",
		/* FM */
		.codec_dai_name = "tlv320aic31xx-dai",
		.codec_name = "tlv320aic31xx-codec.2-0018",
		.ops = &acclaim_ops,
		.ignore_suspend = 1
	},

	{
		.name = "aic31xx_Voice",
		.stream_name = "Voice",

		/* ABE components - VX-UL & VX-DL */
		.cpu_dai_name = "Voice",
		.platform_name = "omap-pcm-audio",

		.dynamic = 1, /* BE is dynamic */
		.dsp_link = &fe_MM,
		.no_host_mode = SND_SOC_DAI_LINK_OPT_HOST,
	},
	{
		.name = "aic31xx_Tones_Playback",
		.stream_name = "Tone Playback",

		/* ABE components - TONES_DL */
		.cpu_dai_name = "Tones",
		.platform_name = "omap-pcm-audio",

		.dynamic = 1, /* BE is dynamic */
		.dsp_link = &fe_tones,
	},
/*
 * Backend DAIs - i.e. dynamically matched interfaces, invisible to userspace.
 * Matched to above interfaces at runtime, based upon use case.
 */

	{
		.name = OMAP_ABE_BE_MM_EXT0_DL,
		.stream_name = "FM Playback",

		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp-dai.1",
		.platform_name = "aess",

		/* FM */
		.codec_dai_name = "tlv320aic31xx-dai",
		.codec_name = "tlv320aic31xx-codec.2-0018",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.init = acclaim_aic31xx_init,
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,

		.ops = &acclaim_ops,
		.be_id = OMAP_ABE_DAI_MM_FM,

		.ignore_suspend = 0,
	},
	{
		.name = OMAP_ABE_BE_MM_EXT0_UL,
		.stream_name = "FM Capture",

		/* ABE components - MCBSP2 - MM-EXT */
		.cpu_dai_name = "omap-mcbsp-dai.1",
		.platform_name = "aess",

		/* FM */
		.codec_dai_name = "tlv320aic31xx-dai",
		.codec_name = "tlv320aic31xx-codec.2-0018",

		.no_pcm = 1, /* don't create ALSA pcm for this */
		.be_hw_params_fixup = mcbsp_be_hw_params_fixup,

		.ops = &acclaim_ops,
		.be_id = OMAP_ABE_DAI_MM_FM,

		.ignore_suspend = 0,
	},

};

static struct snd_soc_card snd_soc_card_acclaim_abe = {
	.name = "OMAP4_ACCLAIM_ABE",
	.long_name = "OMAP4 Acclaim AIC31xx ABE",
	.dai_link = acclaim_dai_link_abe,
	.num_links = ARRAY_SIZE(acclaim_dai_link_abe),
/*	.stream_event = aic31xx_stream_event, */
};

/*
 * Initialization routine.
 */
static int __init acclaim_soc_init(void)
{
	int ret = 0;
	struct device *dev;
	void __iomem *phymux_base = NULL;
	u32 val;

	DBG(KERN_INFO "omap4-sound: Audio SoC init\n");
	acclaim_snd_device = platform_device_alloc("soc-audio", -1);
	if (!acclaim_snd_device) {
		printk(KERN_INFO "Platform device allocation failed\n");
		return -ENOMEM;
	}
#ifdef CONFIG_ABE_44100
#warning "Configuring the ABE at 44.1 Khz."
#endif
	DBG(KERN_INFO "Found Board EVT 2.1-ABE support Enabled\n");
	platform_set_drvdata(acclaim_snd_device, &snd_soc_card_acclaim_abe);
	ret = platform_device_add(acclaim_snd_device);
	if (ret) {
		printk(KERN_INFO "%s: platform device \
					allocation failed\n", __func__);
		goto err1;
	}
	DBG(KERN_INFO "platform device added\n");

	/*
	 * Enable the GPIO related code-base on the ACCLAIM Board for
	 * Headphone/MIC Detection
	 */
	phymux_base = ioremap(0x4a100000, 0x1000);
	val = __raw_readl(phymux_base + 0x90);
	val =  ((val & 0xFEFCFFFE) | 0x01030003);
	__raw_writel(val, phymux_base + 0x90);
	iounmap(phymux_base);
	DBG("exiting %s\n", __func__);
	return 0;

err1:
	printk(KERN_ERR "Unable to add platform device\n");
	platform_device_put(acclaim_snd_device);

	return ret;
}

/*
 * shutdown routine.
 */
static void __exit acclaim_soc_exit(void)
{
	wake_lock_destroy(&omap_wakelock);
	platform_device_unregister(acclaim_snd_device);
}

module_init(acclaim_soc_init);
module_exit(acclaim_soc_exit);

MODULE_AUTHOR("Santosh Sivaraj <santosh.s@mistralsolutions.com>");
MODULE_DESCRIPTION("ALSA SoC for Acclaim Board");
MODULE_LICENSE("GPL");
