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

#ifndef SG_H
#define SG_H

#include "it8951.h"
#include "sf.h"

void it8951_sg_info(struct it8951_data *data);
int it8951_sg_sf_erase(struct it8951_data *data, struct sf *sf,
		       uint32_t sfaddr, uint32_t size);
int it8951_sg_sf_read(struct it8951_data *data, struct sf *sf,
		      uint32_t sfaddr, uint32_t memaddr, uint32_t size);
int it8951_sg_sf_write(struct it8951_data *data, struct sf *sf,
		       uint32_t sfaddr, uint32_t memaddr, uint32_t size);
int it8951_sg_pmic(struct it8951_data *data, uint16_t *vcom, uint8_t *pwr);
int it8951_sg_read_mem(struct it8951_data *data, uint32_t memaddr,
		       char *buffer, size_t size);
int it8951_sg_write_mem(struct it8951_data *data, uint32_t memaddr,
			const char *buffer, size_t size, bool fast);
int it8951_sg_load_area(struct it8951_data *data, uint32_t memaddr,
			struct image *img, struct zone *zone);
int it8951_sg_display_area(struct it8951_data *data, uint32_t memaddr,
			   uint32_t mode, struct zone *u_zone);
int it8951_sg_open(struct it8951_data **data, const char *devname);
void it8951_sg_close(struct it8951_data *data);

#endif
