/*
 * Copyright (c) 2017-2024, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>

#include <platform_def.h>

#include <arch_helpers.h>
#include <common/bl_common.h>
#include <common/debug.h>
#include <common/interrupt_props.h>
#include <drivers/arm/cci.h>
#include <drivers/arm/gicv2.h>
#include <drivers/arm/pl011.h>
#include <lib/mmio.h>

#include <hi6220.h>
#include <hikey_def.h>
#include <hisi_ipc.h>
#include <hisi_pwrc.h>

#include "hikey_private.h"

static entry_point_info_t bl32_ep_info;
static entry_point_info_t bl33_ep_info;
static console_t console;

/******************************************************************************
 * On a GICv2 system, the Group 1 secure interrupts are treated as Group 0
 * interrupts.
 *****************************************************************************/
static const interrupt_prop_t g0_interrupt_props[] = {
	INTR_PROP_DESC(IRQ_SEC_PHY_TIMER, GIC_HIGHEST_SEC_PRIORITY,
		       GICV2_INTR_GROUP0, GIC_INTR_CFG_LEVEL),
	INTR_PROP_DESC(IRQ_SEC_SGI_0, GIC_HIGHEST_SEC_PRIORITY,
		       GICV2_INTR_GROUP0, GIC_INTR_CFG_LEVEL),
};

/*
 * Ideally `arm_gic_data` structure definition should be a `const` but it is
 * kept as modifiable for overwriting with different GICD and GICC base when
 * running on FVP with VE memory map.
 */
gicv2_driver_data_t hikey_gic_data = {
	.gicd_base = PLAT_ARM_GICD_BASE,
	.gicc_base = PLAT_ARM_GICC_BASE,
	.interrupt_props = g0_interrupt_props,
	.interrupt_props_num = ARRAY_SIZE(g0_interrupt_props),
};

static const int cci_map[] = {
	CCI400_SL_IFACE3_CLUSTER_IX,
	CCI400_SL_IFACE4_CLUSTER_IX
};

entry_point_info_t *bl31_plat_get_next_image_ep_info(uint32_t type)
{
	entry_point_info_t *next_image_info;

	next_image_info = (type == NON_SECURE) ? &bl33_ep_info : &bl32_ep_info;

	/* None of the images on this platform can have 0x0 as the entrypoint */
	if (next_image_info->pc)
		return next_image_info;
	return NULL;
}

void bl31_early_platform_setup2(u_register_t arg0, u_register_t arg1,
				u_register_t arg2, u_register_t arg3)
{
	void *from_bl2;

	from_bl2 = (void *) arg0;

	/* Initialize the console to provide early debug support */
	console_pl011_register(CONSOLE_BASE, PL011_UART_CLK_IN_HZ,
			       PL011_BAUDRATE, &console);

	/* Initialize CCI driver */
	cci_init(CCI400_BASE, cci_map, ARRAY_SIZE(cci_map));
	cci_enable_snoop_dvm_reqs(MPIDR_AFFLVL1_VAL(read_mpidr_el1()));

	/*
	 * Check params passed from BL2 should not be NULL,
	 */
	bl_params_t *params_from_bl2 = (bl_params_t *)from_bl2;
	assert(params_from_bl2 != NULL);
	assert(params_from_bl2->h.type == PARAM_BL_PARAMS);
	assert(params_from_bl2->h.version >= VERSION_2);

	bl_params_node_t *bl_params = params_from_bl2->head;

	/*
	 * Copy BL33 and BL32 (if present), entry point information.
	 * They are stored in Secure RAM, in BL2's address space.
	 */
	while (bl_params) {
		if (bl_params->image_id == BL32_IMAGE_ID)
			bl32_ep_info = *bl_params->ep_info;

		if (bl_params->image_id == BL33_IMAGE_ID)
			bl33_ep_info = *bl_params->ep_info;

		bl_params = bl_params->next_params_info;
	}

	if (bl33_ep_info.pc == 0)
		panic();
}

void bl31_plat_arch_setup(void)
{
	hikey_init_mmu_el3(BL31_BASE,
			   BL31_LIMIT - BL31_BASE,
			   BL_CODE_BASE,
			   BL_CODE_END,
			   BL_COHERENT_RAM_BASE,
			   BL_COHERENT_RAM_END);
}

/* Initialize EDMAC controller with non-secure mode. */
static void hikey_edma_init(void)
{
	int i;
	uint32_t non_secure;

	non_secure = EDMAC_SEC_CTRL_INTR_SEC | EDMAC_SEC_CTRL_GLOBAL_SEC;
	mmio_write_32(EDMAC_SEC_CTRL, non_secure);

	for (i = 0; i < EDMAC_CHANNEL_NUMS; i++) {
		mmio_write_32(EDMAC_AXI_CONF(i), (1 << 6) | (1 << 18));
	}
}

void bl31_platform_setup(void)
{
	/* Initialize the GIC driver, cpu and distributor interfaces */
	gicv2_driver_init(&hikey_gic_data);
	gicv2_distif_init();
	gicv2_pcpu_distif_init();
	gicv2_cpuif_enable();

	hikey_edma_init();

	hisi_ipc_init();
	hisi_pwrc_setup();
}
