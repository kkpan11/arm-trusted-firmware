/*
 * Copyright (c) 2017-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>
#include <common_def.h>
#include <context.h>
#include <context_mgmt.h>
#include <debug.h>
#include <platform_def.h>
#include <platform.h>
#include <sp_res_desc.h>
#include <string.h>
#include <xlat_tables_v2.h>

#include "spm_private.h"
#include "spm_shim_private.h"

/* Setup context of the Secure Partition */
void spm_sp_setup(sp_context_t *sp_ctx)
{
	cpu_context_t *ctx = &(sp_ctx->cpu_ctx);

	/*
	 * Initialize CPU context
	 * ----------------------
	 */

	entry_point_info_t ep_info = {0};

	SET_PARAM_HEAD(&ep_info, PARAM_EP, VERSION_1, SECURE | EP_ST_ENABLE);

	/* Setup entrypoint and SPSR */
	ep_info.pc = sp_ctx->rd.attribute.entrypoint;
	ep_info.spsr = SPSR_64(MODE_EL0, MODE_SP_EL0, DISABLE_ALL_EXCEPTIONS);

	/*
	 * X0: Unused (MBZ).
	 * X1: Unused (MBZ).
	 * X2: cookie value (Implementation Defined)
	 * X3: cookie value (Implementation Defined)
	 * X4 to X7 = 0
	 */
	ep_info.args.arg0 = 0;
	ep_info.args.arg1 = 0;
	ep_info.args.arg2 = PLAT_SPM_COOKIE_0;
	ep_info.args.arg3 = PLAT_SPM_COOKIE_1;

	cm_setup_context(ctx, &ep_info);

	/*
	 * Setup translation tables
	 * ------------------------
	 */

	/* This region contains the exception vectors used at S-EL1. */
	const mmap_region_t sel1_exception_vectors =
		MAP_REGION_FLAT(SPM_SHIM_EXCEPTIONS_START,
				SPM_SHIM_EXCEPTIONS_SIZE,
				MT_CODE | MT_SECURE | MT_PRIVILEGED);
	mmap_add_region_ctx(sp_ctx->xlat_ctx_handle,
			    &sel1_exception_vectors);

	mmap_add_ctx(sp_ctx->xlat_ctx_handle,
		     plat_get_secure_partition_mmap(NULL));

	init_xlat_tables_ctx(sp_ctx->xlat_ctx_handle);

	/*
	 * MMU-related registers
	 * ---------------------
	 */
	xlat_ctx_t *xlat_ctx = sp_ctx->xlat_ctx_handle;

	uint64_t mmu_cfg_params[MMU_CFG_PARAM_MAX];

	setup_mmu_cfg((uint64_t *)&mmu_cfg_params, 0, xlat_ctx->base_table,
		      xlat_ctx->pa_max_address, xlat_ctx->va_max_address,
		      EL1_EL0_REGIME);

	write_ctx_reg(get_sysregs_ctx(ctx), CTX_MAIR_EL1,
		      mmu_cfg_params[MMU_CFG_MAIR]);

	write_ctx_reg(get_sysregs_ctx(ctx), CTX_TCR_EL1,
		      mmu_cfg_params[MMU_CFG_TCR]);

	write_ctx_reg(get_sysregs_ctx(ctx), CTX_TTBR0_EL1,
		      mmu_cfg_params[MMU_CFG_TTBR0]);

	/* Setup SCTLR_EL1 */
	u_register_t sctlr_el1 = read_ctx_reg(get_sysregs_ctx(ctx), CTX_SCTLR_EL1);

	sctlr_el1 |=
		/*SCTLR_EL1_RES1 |*/
		/* Don't trap DC CVAU, DC CIVAC, DC CVAC, DC CVAP, or IC IVAU */
		SCTLR_UCI_BIT							|
		/* RW regions at xlat regime EL1&0 are forced to be XN. */
		SCTLR_WXN_BIT							|
		/* Don't trap to EL1 execution of WFI or WFE at EL0. */
		SCTLR_NTWI_BIT | SCTLR_NTWE_BIT					|
		/* Don't trap to EL1 accesses to CTR_EL0 from EL0. */
		SCTLR_UCT_BIT							|
		/* Don't trap to EL1 execution of DZ ZVA at EL0. */
		SCTLR_DZE_BIT							|
		/* Enable SP Alignment check for EL0 */
		SCTLR_SA0_BIT							|
		/* Allow cacheable data and instr. accesses to normal memory. */
		SCTLR_C_BIT | SCTLR_I_BIT					|
		/* Alignment fault checking enabled when at EL1 and EL0. */
		SCTLR_A_BIT							|
		/* Enable MMU. */
		SCTLR_M_BIT
	;

	sctlr_el1 &= ~(
		/* Explicit data accesses at EL0 are little-endian. */
		SCTLR_E0E_BIT							|
		/* Accesses to DAIF from EL0 are trapped to EL1. */
		SCTLR_UMA_BIT
	);

	write_ctx_reg(get_sysregs_ctx(ctx), CTX_SCTLR_EL1, sctlr_el1);

	/*
	 * Setup other system registers
	 * ----------------------------
	 */

	/* Shim Exception Vector Base Address */
	write_ctx_reg(get_sysregs_ctx(ctx), CTX_VBAR_EL1,
			SPM_SHIM_EXCEPTIONS_PTR);

	/*
	 * FPEN: Allow the Secure Partition to access FP/SIMD registers.
	 * Note that SPM will not do any saving/restoring of these registers on
	 * behalf of the SP. This falls under the SP's responsibility.
	 * TTA: Enable access to trace registers.
	 * ZEN (v8.2): Trap SVE instructions and access to SVE registers.
	 */
	write_ctx_reg(get_sysregs_ctx(ctx), CTX_CPACR_EL1,
			CPACR_EL1_FPEN(CPACR_EL1_FP_TRAP_NONE));
}
