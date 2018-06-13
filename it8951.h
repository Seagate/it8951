/*
 * This file is part of the it8951 collection of tools.
 *
 * Copyright (C) 2018-2020 Seagate Technology LLC
 *
 * it8951 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * it8951 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with it8951.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IT8951_H
#define IT8951_H

#include <stdint.h>
#include <scsi/sg.h>

#include "image.h"

struct it8951_device {
	uint32_t std_cmd_num;		/* Standard command number2T-con communication protocol */
	uint32_t ext_cmd_num;		/* Extend command number */
	uint32_t signature;		/* 31 35 39 38h (8951) */
	uint32_t version;		/* Command table version */
	uint32_t width;			/* Panel width */
	uint32_t height;		/* Panel height */
	uint32_t update_memaddr;	/* Update buffer address */
	uint32_t memaddr;		/* Image buffer address (index 0) */
	uint32_t temp_seg_num;		/* Temperature segment number */
	uint32_t mode;			/* Display mode number */
	uint32_t frame_count[8];	/* Frame count for each mode. */
	uint32_t buf_num;		/* Number of image buffers. */
	uint32_t unused[9];
	void* cmd_table[];		/* Command table pointer */
} __attribute__((packed));

struct zone {
	int x;
	int y;
	int width;
	int height;
};

struct it8951_data {
	int			fd;
	struct it8951_device	*dev;
	struct sg_io_hdr	*sg_hdr;
};
#endif
