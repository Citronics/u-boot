// SPDX-License-Identifier: GPL-2.0
/*
 * Stub clk driver for non-essential clocks.
 *
 * This driver should be used for clock controllers
 * which are described as dependencies in DT but aren't
 * actually necessary for hardware functionality.
 */

#include <clk-uclass.h>
#include <dm.h>
#include <asm/global_data.h>
#include <dm/lists.h>

DECLARE_GLOBAL_DATA_PTR;

/* NOP parent nodes to stub clocks */
static const struct udevice_id nop_parent_ids[] = {
	{ .compatible = "qcom,rpm-proc" },
	{ .compatible = "qcom,glink-rpm" },
	{ .compatible = "qcom,glink-smd-rpm" },
	{ .compatible = "qcom,smd-rpm" },
	{ }
};

static int nop_parent_bind(struct udevice *dev)
{
	bool pre_reloc_only = !(gd->flags & GD_FLG_RELOC);
	ofnode edge, node;
	int ret;

	ret = dm_scan_fdt_dev(dev);
	if (ret)
		return ret;

	/* The SMD edge of an RPM has no compatible, bind its children directly */
	edge = dev_read_subnode(dev, "smd-edge");
	if (!ofnode_valid(edge))
		return 0;

	ofnode_for_each_subnode(node, edge) {
		if (!ofnode_is_enabled(node))
			continue;
		ret = lists_bind_fdt(dev, node, NULL, NULL, pre_reloc_only);
		if (ret)
			return ret;
	}

	return 0;
}

U_BOOT_DRIVER(nop_parent) = {
	.name = "nop_parent",
	.id = UCLASS_NOP,
	.of_match = nop_parent_ids,
	.bind = nop_parent_bind,
	.flags = DM_FLAG_DEFAULT_PD_CTRL_OFF,
};

static ulong stub_clk_set_rate(struct clk *clk, ulong rate)
{
	return (clk->rate = rate);
}

static ulong stub_clk_get_rate(struct clk *clk)
{
	return clk->rate;
}

static int stub_clk_nop(struct clk *clk)
{
	return 0;
}

static struct clk_ops stub_clk_ops = {
	.set_rate = stub_clk_set_rate,
	.get_rate = stub_clk_get_rate,
	.enable = stub_clk_nop,
	.disable = stub_clk_nop,
};

static const struct udevice_id stub_clk_ids[] = {
	{ .compatible = "qcom,qcs615-rpmh-clk" },
	{ .compatible = "qcom,rpmcc" },
	{ .compatible = "qcom,sa8775p-rpmh-clk" },
	{ .compatible = "qcom,sc7180-rpmh-clk" },
	{ .compatible = "qcom,sc7280-rpmh-clk" },
	{ .compatible = "qcom,sdm670-rpmh-clk" },
	{ .compatible = "qcom,sdm845-rpmh-clk" },
	{ .compatible = "qcom,sm6350-rpmh-clk" },
	{ .compatible = "qcom,sm8150-rpmh-clk" },
	{ .compatible = "qcom,sm8250-rpmh-clk" },
	{ .compatible = "qcom,sm8550-rpmh-clk" },
	{ .compatible = "qcom,sm8650-rpmh-clk" },
	{ }
};

U_BOOT_DRIVER(clk_stub) = {
	.name = "clk_stub",
	.id = UCLASS_CLK,
	.ops = &stub_clk_ops,
	.of_match = stub_clk_ids,
	.flags = DM_FLAG_DEFAULT_PD_CTRL_OFF,
};

