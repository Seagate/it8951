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

#ifndef FW_H
#define FW_H

#define FW_MAX_BS 12

struct fw_info {
	char *ver_str;
	int ver_maj;
	int ver_min;
	bool have_bs;
	unsigned int bs_num;
	int bs_act;
	uint32_t bs_addr[FW_MAX_BS];
};

int fw_get_info(struct it8951_data *data, uint32_t memaddr,
		struct fw_info **fw_info);
void fw_put_info(struct fw_info *fw_info);
void fw_print_info(struct fw_info *fw_info);
int fw_write_img(struct it8951_data *data, uint32_t memaddr,
		 char *fw, uint32_t size);
int fw_write_bs(struct it8951_data *data, uint32_t memaddr,
		struct fw_info *fw_info, char *bs, uint32_t size,
		unsigned int index);
int fw_enable_bs(struct it8951_data *data, uint32_t memaddr,
		 struct fw_info *fw_info, unsigned int index);

#endif
