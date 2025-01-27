/*
 * Copyright (c) 2023-2025, Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stddef.h>
#include <arch_helpers.h>
#include <common/debug.h>
#include <lib/transfer_list.h>
#include <platform_def.h>


static struct transfer_list_header *tl_hdr;
static int32_t tl_ops_holder;

bool populate_data_from_xfer_list(void)
{
	bool ret = true;

	tl_hdr = (struct transfer_list_header *)FW_HANDOFF_BASE;
	tl_ops_holder = transfer_list_check_header(tl_hdr);

	if ((tl_ops_holder != TL_OPS_ALL) && (tl_ops_holder != TL_OPS_RO)) {
		ret = false;
	}

	return ret;
}

int32_t transfer_list_populate_ep_info(entry_point_info_t *bl32,
				       entry_point_info_t *bl33)
{
	int32_t ret = tl_ops_holder;
	struct transfer_list_entry *te = NULL;
	struct entry_point_info *ep = NULL;

	if ((tl_ops_holder == TL_OPS_ALL) || (tl_ops_holder == TL_OPS_RO)) {
		transfer_list_dump(tl_hdr);
		while ((te = transfer_list_next(tl_hdr, te)) != NULL) {
			ep = transfer_list_entry_data(te);
			if (te->tag_id == TL_TAG_EXEC_EP_INFO64) {
				switch (GET_SECURITY_STATE(ep->h.attr)) {
				case NON_SECURE:
					*bl33 = *ep;
					continue;
				case SECURE:
					*bl32 = *ep;
					if (!transfer_list_set_handoff_args(tl_hdr, ep)) {
						ERROR("Invalid transfer list\n");
					}
					continue;
				default:
					ERROR("Unrecognized Image Security State %lu\n",
					      GET_SECURITY_STATE(ep->h.attr));
					ret = TL_OPS_NON;
				}
			}
		}
	}

	return ret;
}

void *transfer_list_retrieve_dt_address(void)
{
	void *dtb = NULL;
	struct transfer_list_entry *te = NULL;

	if ((tl_ops_holder == TL_OPS_ALL) || (tl_ops_holder == TL_OPS_RO)) {
		te = transfer_list_find(tl_hdr, TL_TAG_FDT);
		if (te != NULL) {
			dtb = transfer_list_entry_data(te);
		}
	}

	return dtb;
}
