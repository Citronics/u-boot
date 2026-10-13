// SPDX-License-Identifier: GPL-2.0+
/*
 * Framebuffer driver for the display that the Android bootloader leaves
 * running on Qualcomm SoCs with an MDP5 display controller.
 *
 * The scanout buffer is found from the source registers of the active source
 * pipe. Packed 24 bpp buffers are converted to XRGB8888, which U-Boot can
 * draw into. Panels driven in DSI command mode only show new content when a
 * frame is pushed, so each update starts one.
 */

#include <cpu_func.h>
#include <dm.h>
#include <log.h>
#include <time.h>
#include <video.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/sizes.h>

#define MDSS_HW_VERSION			0x00
#define MDSS_HW_VERSION_MINOR		GENMASK(27, 16)

#define CTL_LAYER0			0x00
#define CTL_TOP				0x14
#define CTL_TOP_CMD_MODE		BIT(17)
#define CTL_FLUSH			0x18
#define CTL_START			0x1c

#define SSPP_SRC_SIZE			0x00
#define SSPP_SRC_IMG_SIZE		0x04
#define SSPP_SRC_ADDR0			0x14
#define SSPP_SRC_YSTRIDE		0x24
#define SSPP_SRC_FORMAT			0x30
#define SSPP_SRC_UNPACK_PATTERN		0x34

#define SSPP_SRC_FORMAT_BPP		GENMASK(10, 9)

#define SSPP_FORMAT_XRGB8888		(GENMASK(10, 9) | GENMASK(7, 0) | \
					 BIT(17) | FIELD_PREP(GENMASK(13, 12), 3))
/* Memory byte order B, G, R, X: a little-endian x8r8g8b8 word */
#define SSPP_UNPACK_XRGB		0x03020001

#define MDP_X(val)			((val) & 0xffff)
#define MDP_Y(val)			((val) >> 16)

struct mdp5_pipe {
	const char *name;
	u32 flush_bit;
};

/* Only pipes that exist on every MDP5 revision: other offsets may trap */
static const struct mdp5_pipe mdp5_pipes[] = {
	{ "VIG0", BIT(0) },
	{ "RGB0", BIT(3) },
	{ "DMA0", BIT(11) },
};

/* Offsets from the MDSS base ("mdss_phys") */
struct mdp5_layout {
	u32 ctl0;
	u32 pipe[ARRAY_SIZE(mdp5_pipes)];
};

/* MDSS 1.0 to 1.3 (MSM8974, APQ8084) */
static const struct mdp5_layout mdp5_layout_v1_0 = {
	.ctl0 = 0x00600,
	.pipe = { 0x01200, 0x01e00, 0x02a00 },
};

/* MDSS 1.6 and later (MSM8916, MSM8953, ...) */
static const struct mdp5_layout mdp5_layout_v1_6 = {
	.ctl0 = 0x02000,
	.pipe = { 0x05000, 0x15000, 0x25000 },
};

struct mdp5_splash_priv {
	void __iomem *ctl;
	bool cmd_mode;
	ulong last_kick;
};

/* Minimum time between two frames pushed to a command mode panel */
#define MDP5_KICK_MS			30

/*
 * Expand a packed 24 bpp buffer to 32 bpp in place. Walking backwards keeps
 * every source pixel intact until it has been read.
 */
static void mdp5_expand_rgb888(void *fb, u32 npixels)
{
	const u8 *src = fb;
	u32 *dst = fb;
	u32 i = npixels;

	while (i--)
		dst[i] = src[3 * i] | src[3 * i + 1] << 8 | src[3 * i + 2] << 16;
}

/*
 * Push a frame to a command mode panel. This runs before the uclass flushes
 * the cache, so flush the framebuffer here first.
 */
static void mdp5_splash_kick(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct mdp5_splash_priv *priv = dev_get_priv(dev);

	flush_dcache_range(plat->base, ALIGN(plat->base + plat->size, SZ_4K));
	writel(1, priv->ctl + CTL_START);
	priv->last_kick = get_timer(0);
}

static int mdp5_splash_sync(struct udevice *dev)
{
	struct mdp5_splash_priv *priv = dev_get_priv(dev);

	if (!priv->cmd_mode || get_timer(priv->last_kick) < MDP5_KICK_MS)
		return 0;

	mdp5_splash_kick(dev);

	return 0;
}

/*
 * Updates are rate limited, so the last lines printed before the OS starts
 * may not have been pushed yet: push them once the previous frame is out.
 */
static int mdp5_splash_remove(struct udevice *dev)
{
	struct mdp5_splash_priv *priv = dev_get_priv(dev);

	if (!priv->cmd_mode)
		return 0;

	mdelay(MDP5_KICK_MS);
	mdp5_splash_kick(dev);
	mdelay(MDP5_KICK_MS);

	return 0;
}

static int mdp5_splash_probe(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct mdp5_splash_priv *priv = dev_get_priv(dev);
	const struct mdp5_layout *layout;
	const struct mdp5_pipe *pipe = NULL;
	u32 img_size, stride, format, bpp, width, height, addr = 0;
	phys_addr_t base;
	void __iomem *regs;
	ulong size;
	int i;

	base = dev_read_addr_name(dev, "mdss_phys");
	if (base == FDT_ADDR_T_NONE)
		return -EINVAL;

	if (FIELD_GET(MDSS_HW_VERSION_MINOR, readl(base + MDSS_HW_VERSION)) < 6)
		layout = &mdp5_layout_v1_0;
	else
		layout = &mdp5_layout_v1_6;

	for (i = 0; i < ARRAY_SIZE(mdp5_pipes); i++) {
		addr = readl(base + layout->pipe[i] + SSPP_SRC_ADDR0);
		if (addr) {
			pipe = &mdp5_pipes[i];
			break;
		}
	}

	if (!pipe) {
		log_debug("%s: no active MDP5 pipe\n", dev->name);
		return -ENODEV;
	}

	priv->ctl = (void __iomem *)(base + layout->ctl0);
	priv->cmd_mode = readl(priv->ctl + CTL_TOP) & CTL_TOP_CMD_MODE;

	regs = (void __iomem *)(base + layout->pipe[i]);
	img_size = readl(regs + SSPP_SRC_IMG_SIZE);
	stride = readl(regs + SSPP_SRC_YSTRIDE) & 0xffff;
	format = readl(regs + SSPP_SRC_FORMAT);
	bpp = FIELD_GET(SSPP_SRC_FORMAT_BPP, format) + 1;
	width = MDP_X(img_size);
	height = MDP_Y(img_size);

	log_debug("%s: pipe %s fb %#x %ux%u stride %u format %#x%s\n",
		  dev->name, pipe->name, addr, width, height, stride, format,
		  priv->cmd_mode ? " cmd mode" : "");

	if (!width || !height || bpp < 2) {
		log_err("%s: unsupported splash mode %ux%u %u bpp\n", dev->name,
			width, height, bpp);
		return -EINVAL;
	}

	size = ALIGN(max(stride, width * 4) * height, SZ_4K);
#ifdef CONFIG_ARM64
	mmu_map_region(addr, size, false);
#endif
	invalidate_dcache_range(addr, addr + size);

	if (bpp == 3) {
		mdp5_expand_rgb888((void *)(uintptr_t)addr, stride / 3 * height);
		stride = stride / 3 * 4;
		flush_dcache_range(addr, addr + size);

		writel(SSPP_FORMAT_XRGB8888, regs + SSPP_SRC_FORMAT);
		writel(SSPP_UNPACK_XRGB, regs + SSPP_SRC_UNPACK_PATTERN);
		writel(stride, regs + SSPP_SRC_YSTRIDE);
		writel(pipe->flush_bit, priv->ctl + CTL_FLUSH);
		bpp = 4;
	}

	plat->base = addr;
	plat->size = stride * height;

	uc_priv->xsize = width;
	uc_priv->ysize = height;
	uc_priv->line_length = stride;
	if (bpp == 2) {
		uc_priv->bpix = VIDEO_BPP16;
	} else {
		uc_priv->bpix = VIDEO_BPP32;
		uc_priv->format = VIDEO_X8R8G8B8;
	}

	video_set_flush_dcache(dev, true);
	priv->last_kick = get_timer(0) - MDP5_KICK_MS;
	mdp5_splash_sync(dev);

	return 0;
}

static const struct video_ops mdp5_splash_ops = {
	.video_sync = mdp5_splash_sync,
};

static const struct udevice_id mdp5_splash_ids[] = {
	{ .compatible = "qcom,mdss" },
	{ }
};

U_BOOT_DRIVER(qcom_mdp5_splash) = {
	.name	= "qcom_mdp5_splash",
	.id	= UCLASS_VIDEO,
	.of_match = mdp5_splash_ids,
	.ops	= &mdp5_splash_ops,
	.probe	= mdp5_splash_probe,
	.remove	= mdp5_splash_remove,
	.priv_auto = sizeof(struct mdp5_splash_priv),
	/* The display is already powered and clocked; leave it alone. */
	.flags	= DM_FLAG_DEFAULT_PD_CTRL_OFF | DM_FLAG_OS_PREPARE,
};
