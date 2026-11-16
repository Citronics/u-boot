// SPDX-License-Identifier: GPL-2.0+
/*
 * Caches on 32-bit (ARMv7) Snapdragon SoCs.
 *
 * DRAM is mapped write-back cacheable in 1 MiB sections, except the sections
 * that touch a reserved-memory region marked no-map: those hold the secure
 * world's and the remote processors' memory, which the XPUs protect, and a
 * speculative or prefetched access to them hangs the SoC until the secure
 * watchdog resets it. Like everything outside DRAM, they stay strongly
 * ordered and execute-never, which the CPU never accesses speculatively.
 */

#include <asm/cache.h>
#include <asm/global_data.h>
#include <asm/system.h>
#include <cpu_func.h>
#include <fdtdec.h>
#include <linux/kernel.h>
#include <linux/libfdt.h>

DECLARE_GLOBAL_DATA_PTR;

#define MAX_NOMAP_REGIONS 32

static struct {
	phys_addr_t start;
	phys_addr_t end;
} nomap[MAX_NOMAP_REGIONS];
static int nomap_count;

static void qcom_find_nomap_regions(void)
{
	int parent, node;

	parent = fdt_path_offset(gd->fdt_blob, "/reserved-memory");
	if (parent < 0)
		return;

	fdt_for_each_subnode(node, gd->fdt_blob, parent) {
		struct fdt_resource res;

		if (!fdt_getprop(gd->fdt_blob, node, "no-map", NULL))
			continue;
		if (fdt_get_resource(gd->fdt_blob, node, "reg", 0, &res))
			continue;
		if (nomap_count == MAX_NOMAP_REGIONS) {
			log_err("Too many no-map regions, caches stay off\n");
			nomap_count = -1;
			return;
		}
		nomap[nomap_count].start = res.start;
		nomap[nomap_count].end = res.end + 1;
		nomap_count++;
	}
}

static bool qcom_section_is_nomap(phys_addr_t start)
{
	phys_addr_t end = start + MMU_SECTION_SIZE;
	int i;

	for (i = 0; i < nomap_count; i++)
		if (nomap[i].start < end && nomap[i].end > start)
			return true;

	return false;
}

void dram_bank_mmu_setup(int bank)
{
	phys_addr_t start, end, addr;

	/* gd->dram is available only after relocation */
	if (!(gd->flags & GD_FLG_RELOC))
		return;

	start = ALIGN_DOWN(gd->dram[bank].start, MMU_SECTION_SIZE);
	end = gd->dram[bank].start + gd->dram[bank].size;
	for (addr = start; addr < end; addr += MMU_SECTION_SIZE)
		set_section_dcache(addr >> MMU_SECTION_SHIFT,
				   qcom_section_is_nomap(addr) ? DCACHE_OFF :
								 DCACHE_DEFAULT_OPTION);
}

void enable_caches(void)
{
	qcom_find_nomap_regions();
	if (nomap_count < 0)
		return;

	icache_enable();
	dcache_enable();
}
