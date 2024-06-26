/*
 * Copyright (c) 2021-2024, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stdint.h>

#include <common/desc_image_load.h>
#include <drivers/measured_boot/event_log/event_log.h>
#include <plat/arm/common/plat_arm.h>
#include <plat/common/platform.h>

extern event_log_metadata_t fvp_event_log_metadata[];

int plat_mboot_measure_image(unsigned int image_id, image_info_t *image_data)
{
	int err;

	/* Calculate image hash and record data in Event Log */
	err = event_log_measure_and_record(image_data->image_base,
					   image_data->image_size,
					   image_id,
					   fvp_event_log_metadata);
	if (err != 0) {
		ERROR("%s%s image id %u (%i)\n",
		      "Failed to ", "record in event log", image_id, err);
		return err;
	}

	return 0;
}

int plat_mboot_measure_key(const void *pk_oid, const void *pk_ptr,
			   size_t pk_len)
{
	return 0;
}
