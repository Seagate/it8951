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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "debug.h"
#include "sf.h"
#include "fw.h"

/*
 * Firmware version 0.2
 *
 * This version introduces support for an image library called "imglib". This
 * library allows to store a boot screen image in the firmware. This image is
 * automatically displayed at power-up.
 *
 * Although "imglib" seems designed to store several images, a single image is
 * supported. The known firmware versions are 0.2, 0.2T1 and 0.2T2.
 */

struct imglib_hdr {
	char imagelib_magic[32];
	uint16_t num_img;
	char unknown[14];
	uint16_t index;
	uint16_t bpp;
	uint32_t offset;
	uint16_t width;
	uint16_t height;
};

static int fw_0_2_get_info(struct it8951_data *data, uint32_t memaddr,
			   struct fw_info *fw_info)
{
	uint32_t size = 512 * 1024;
	char *fw;
	int ret;
	const char hdr_magic[] = "IT8951_ImageLib";
	struct imglib_hdr *hdr;

	/* Read the first 512KB of the firmware image from flash. */
	fw = malloc(size);
	if (!fw) {
		err("Failed to malloc %d bytes: %s\n", size, strerror(errno));
		return ENOMEM;
	}

	ret = sf_read(data, memaddr, 0, size, fw);
	if (ret)
		goto exit_free;

	ret = EINVAL;

	/* Find imglib header in firmware image. */
	hdr = memmem(fw, size, hdr_magic, strlen(hdr_magic));
	if (!hdr) {
		err("Imglib header not found\n");
		goto exit_free;
	}

	info("fw: found imglib header\n");
	debug("num_img: %d\n", be16toh(hdr->num_img));
	debug("index  : %d\n", be16toh(hdr->index));
	debug("bpp    : %d\n", be16toh(hdr->bpp));
	debug("offset : %d\n", be32toh(hdr->offset));
	debug("width  : %d\n", be16toh(hdr->width));
	debug("height : %d\n", be16toh(hdr->height));

	/* Check imglib header. */
	if (be16toh(hdr->num_img) != 1) {
		err("Invalid header: num_img=%d (should be 1)\n",
		    be16toh(hdr->num_img));
		goto exit_free;
	}
	if (be16toh(hdr->index)) {
		err("Invalid header: index=%d (should be 0)\n",
		    be16toh(hdr->index));
		goto exit_free;
	}
	if (be16toh(hdr->bpp) != 8) {
		err("Invalid header: bpp=%d (should be 8)\n",
		    be16toh(hdr->bpp));
		goto exit_free;
	}
	if (be16toh(hdr->width) != data->dev->width) {
		err("Display width (%d) don't match header (%d)\n",
		    data->dev->width, be16toh(hdr->width));
		goto exit_free;
	}
	if (be16toh(hdr->height) != data->dev->height) {
		err("Display height (%d) don't match header (%d)\n",
		    data->dev->height, be16toh(hdr->height));
		goto exit_free;
	}

	fw_info->have_bs = true;
	fw_info->bs_addr[0] =
		(uint32_t) ((char *) hdr - fw + be32toh(hdr->offset));
	fw_info->bs_act = 0;
	fw_info->bs_num = 1;

	ret = 0;

exit_free:
        free(fw);
	return ret;
}

/*
 * Firmware version 0.3
 *
 * This version introduces support for multiple boot screen images.
 *
 * A "switch block" can be found at the 0x170000 address. This "switch block"
 * holds the address of the active boot screen image. The boot screen images
 * are stored at addresses starting from 0x180000 and until the end of the flash.
 * Images addresses must be 64KB aligned and the images should not be overlapped.
 *
 * At startup the IT8951 firmware retrieves the boot screen image address from
 * the switch block" (at 0x170000) and automatically displays the image.
 */

#define BS_SWITCH_ADDR 0x170000
#define BS_START_ADDR 0x180000
#define BS_SWITCH_TAG "LOGO_"

static int fw_0_3_get_info(struct it8951_data *data, uint32_t memaddr,
			   struct fw_info *fw_info)
{
	int ret;
	char buffer[64];
	int img_size;
	int i;
	uint32_t addr;

	fw_info->have_bs = true;

	/*
	 * Compute flash layout (i.e. addresses of all the boot screen images).
	 */
	i = 0;
	addr = BS_START_ADDR;
	img_size = data->dev->width * data->dev->height;

	while ((addr + img_size < SF_SIZE) && (i < FW_MAX_BS)) {
		fw_info->bs_addr[i++] = addr;
		addr = sf_block_align_next(addr + img_size);
	}
	fw_info->bs_num = i;

	/*
	 * Read the "switch block" to retrieve the address of the active boot
	 * screen image.
	 *
	 * Here is an example of "switch block" with if the boot screen image
	 * address set to 0x200000:
	 *
	 * 00000000: 4c4f 474f 5f20 0000 ffff ffff ffff ffff  LOGO_ ..........
	 */
	ret = sf_read(data, memaddr, BS_SWITCH_ADDR, sizeof(buffer), buffer);
        if (ret)
		return ret;

	ret = memcmp(buffer, BS_SWITCH_TAG, strlen(BS_SWITCH_TAG));
	if (ret) {
		info("fw: no switch block tag found\n");
		return 0;
	}

        addr = 0;
        memcpy((void *) ((char *) &addr + 1), buffer + strlen(BS_SWITCH_TAG), 3);
        addr = be32toh(addr);

	info("fw: switch block: boot screen address is 0x%08x\n", addr);

	/*
	 * Convert the active boot screen image address into an index (matching
	 * the flash layout).
	 */
	for (i = 0; i < fw_info->bs_num; i++) {
		if (addr == fw_info->bs_addr[i]) {
			fw_info->bs_act = i;
			break;
		}
	}
	if (fw_info->bs_act == -1)
		info("fw: switch block: boot screen address (0x%08x) don't match layout\n",
		     addr);

	return 0;
}

static int fw_0_3_enable_bs(struct it8951_data *data, uint32_t memaddr,
			    struct fw_info *fw_info, unsigned int index)
{
	char buffer[8];
	uint32_t addr;
	int ret;

	memcpy(buffer, BS_SWITCH_TAG, strlen(BS_SWITCH_TAG));
        addr = htobe32(fw_info->bs_addr[index]);
        memcpy(buffer + strlen(BS_SWITCH_TAG), (void *) ((char *) &addr + 1), 3);

	ret = sf_write(data, memaddr,
		       buffer, sizeof(buffer), BS_SWITCH_ADDR, true);
	if (!ret)
		fw_info->bs_act = index;

	return ret;
}

/*
 * Firmware common functions.
 */

#define FW_TAG "ITEEPD8951_A0100"
#define FW_VERSION_OFFSET 0x120

void fw_print_info(struct fw_info *fw_info)
{
	int i;

	fprintf(stdout, "Firmware version    : %s\n", fw_info->ver_str);
	fprintf(stdout, "Boot screen support : %s\n",
		fw_info->have_bs ? "yes" : "no");
	if (!fw_info->have_bs)
		return;
	fprintf(stdout, "Number of BS images : %d\n", fw_info->bs_num);
	for (i = 0; i < fw_info->bs_num; i++)
		fprintf(stdout, "BS image %d address  : 0x%08x\n",
			i, fw_info->bs_addr[i]);
	if (fw_info->bs_act < 0)
		fprintf(stdout, "Active BS image     : not set\n");
	else
		fprintf(stdout, "Active BS image     : %d\n", fw_info->bs_act);
}

int fw_get_info(struct it8951_data *data, uint32_t memaddr,
		struct fw_info **fw_info)
{
	int ret;
	char buffer[64];
	const char ver_tag[] = "_v.";
	char *ver;
	int ver_maj, ver_min;

	ret = sf_read(data, memaddr, FW_VERSION_OFFSET, sizeof(buffer), buffer);
        if (ret)
		return ret;

	ver = memmem(buffer, sizeof(buffer), ver_tag, strlen(ver_tag));
	if (!ver) {
		err("Failed to find firmware version string\n");
		return EINVAL;
	}

	ret = sscanf(ver, "_v.%d.%d", &ver_maj, &ver_min);
	if (ret != 2) {
		err("Failed to get firmware version number\n");
		return EINVAL;
	}

	*fw_info = calloc(1, sizeof(struct fw_info));
	if (!*fw_info) {
		err("Failed to calloc %ld bytes: %s\n",
		    sizeof(struct fw_info), strerror(errno));
		return ENOMEM;
	}

	(*fw_info)->ver_str = strndup(buffer, sizeof(buffer) - 1);
	if (!(*fw_info)->ver_str) {
		err("Failed to strndup version string: %s\n", strerror(errno));
		ret = ENOMEM;
		goto err_free;
	}
	(*fw_info)->ver_maj = ver_maj;
	(*fw_info)->ver_min = ver_min;
	(*fw_info)->have_bs = false;
	(*fw_info)->bs_num = 0;
	(*fw_info)->bs_act = -1;

	if (ver_maj == 0 && ver_min == 2)
		return fw_0_2_get_info(data, memaddr, *fw_info);

	if (ver_maj == 0 && ver_min >= 3)
		return fw_0_3_get_info(data, memaddr, *fw_info);

	return 0;

err_free:
	free(*fw_info);
	return ret;
}

void fw_put_info(struct fw_info *fw_info)
{
	free(fw_info);
}

int fw_write_img(struct it8951_data *data, uint32_t memaddr,
		 char *fw, uint32_t size)
{
	return sf_write(data, memaddr, fw, size, 0, true);
}

int fw_write_bs(struct it8951_data *data, uint32_t memaddr,
		struct fw_info *fw_info, char *bs, uint32_t size,
		unsigned int index)
{
	if (!fw_info->have_bs) {
		err("Firmware version %s don't support boot screen image\n",
		    fw_info->ver_str);
		return EINVAL;
	}
	if (index >= fw_info->bs_num) {
		err("Invalid boot screen index %d (max=%d)\n",
		    index, fw_info->bs_num - 1);
		return EINVAL;
	}
	if (size != data->dev->width * data->dev->height) {
		err("Boot screen image size (%d bytes) don't match screen resolution (%dx%d)\n",
		    size, data->dev->width, data->dev->height);
		return EINVAL;
	}

	return sf_write(data, memaddr, bs, size, fw_info->bs_addr[index], true);
}

int fw_enable_bs(struct it8951_data *data, uint32_t memaddr,
		 struct fw_info *fw_info, unsigned int index)
{
	if (!fw_info->have_bs) {
		err("Firmware version %s don't support boot screen image\n",
		    fw_info->ver_str);
		return EINVAL;
	}
	if (index > fw_info->bs_num) {
		err("Invalid boot screen index %d (max=%d)\n",
		    index, fw_info->bs_num - 1);
		return EINVAL;
	}

	if (fw_info->ver_maj == 0 && fw_info->ver_min >= 3)
		return fw_0_3_enable_bs(data, memaddr, fw_info, index);

	err("Firmware version %s don't support multiple boot screen images\n",
	    fw_info->ver_str);

	return EINVAL;
}
