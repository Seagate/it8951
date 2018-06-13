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

#ifndef SF_H
#define SF_H

#include "it8951.h"

#define SF_SIZE (64* 64 * 1024) /* 64 blocks of 64KB (4MB) */

struct sf {
	int block_size;
	int n_blocks;
	int sector_size;
};

uint32_t sf_block_align_prev(uint32_t addr);
uint32_t sf_block_align_next(uint32_t addr);
int sf_erase(struct it8951_data *data, uint32_t memaddr,
	     uint32_t addr, uint32_t size);
int sf_read(struct it8951_data *data, uint32_t memaddr,
	    uint32_t addr, uint32_t count, char *buf);
int sf_verify(struct it8951_data *data, uint32_t memaddr,
	      uint32_t addr, uint32_t size, const char *ref);
int sf_write(struct it8951_data *data, uint32_t memaddr,
             const char *buf, uint32_t count, uint32_t addr, bool verify);

#endif
