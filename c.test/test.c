#include <stdio.h>

struct snd_soc_dai_link_component {
	const char *name;
	const char *dai_name;
};

struct snd_soc_dai_link {
	const char *name;			/* Codec name */
	const char *stream_name;		/* Stream name */
	struct snd_soc_dai_link_component *cpus;
	unsigned int num_cpus;
	struct snd_soc_dai_link_component *codecs;
	unsigned int num_codecs;
	struct snd_soc_dai_link_component *platforms;
}

#define SND_SOC_DAILINK_REG1(name)	 SND_SOC_DAILINK_REG3(name##_cpus, name##_codecs, name##_platforms)
#define SND_SOC_DAILINK_REG2(cpu, codec) SND_SOC_DAILINK_REG3(cpu, codec, null_dailink_component)
#define SND_SOC_DAILINK_REG3(cpu, codec, platform)	\
	.cpus		= cpu,				\
	.num_cpus	= ARRAY_SIZE(cpu),		\
	.codecs		= codec,			\
	.num_codecs	= ARRAY_SIZE(codec),		\
	.platforms	= platform,			\
	.num_platforms	= ARRAY_SIZE(platform)

#define SND_SOC_DAILINK_REGx(_1, _2, _3, func, ...) func
#define SND_SOC_DAILINK_REG(...) \
	SND_SOC_DAILINK_REGx(__VA_ARGS__,		\
			SND_SOC_DAILINK_REG3,	\
			SND_SOC_DAILINK_REG2,	\
			SND_SOC_DAILINK_REG1)(__VA_ARGS__)

#define SND_SOC_DAILINK_DEF(name, def...)		\
	static struct snd_soc_dai_link_component name[]	= { def }

#define SND_SOC_DAILINK_DEFS(name, cpu, codec, platform...)	\
	SND_SOC_DAILINK_DEF(name##_cpus, cpu);			\
	SND_SOC_DAILINK_DEF(name##_codecs, codec);		\
	SND_SOC_DAILINK_DEF(name##_platforms, platform)

#define DAILINK_COMP_ARRAY(param...)	param
#define COMP_EMPTY()			{ }
#define COMP_CPU(_dai)			{ .dai_name = _dai, }
#define COMP_CODEC(_name, _dai)		{ .name = _name, .dai_name = _dai, }
#define COMP_PLATFORM(_name)		{ .name = _name }
#define COMP_AUX(_name)			{ .name = _name }
#define COMP_DUMMY()			{ .name = "snd-soc-dummy", .dai_name = "snd-soc-dummy-dai", }


SND_SOC_DAILINK_DEFS(tavil_i2s_rx1,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tavil_codec", "tavil_i2s_rx1"),
			   COMP_CODEC("wsa-codec.1", "wsa_rx1")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));


SND_SOC_DAILINK_DEFS(usb_audio_rx,
	DAILINK_COMP_ARRAY(COMP_CPU("snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd-soc-dummy")));

static struct snd_soc_dai_link msm_rx_tx_cdc_dma_be_dai_links[] = {
	/* WSA CDC DMA Backend DAI Links */
	{
		.name = "MI2S-LPAIF-RX-PRIMARY",
		.stream_name = "MI2S-LPAIF-RX-PRIMARY",
		SND_SOC_DAILINK_REG(tavil_i2s_rx1),
	}
}

static struct snd_soc_dai_link msm_rx_tx_cdc_dma_be_dai_links[] = {
	/* WSA CDC DMA Backend DAI Links */
	{
		.name = "LPASS_BE_USB_AUDIO_RX",
		.stream_name = "LPASS_BE_USB_AUDIO_RX",
		SND_SOC_DAILINK_REG(usb_audio_rx),
	}
}