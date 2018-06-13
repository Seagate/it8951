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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "debug.h"
#include "sg.h"

#define IT8951_CMD_CUSTOMER		0xfe
#define IT8951_CMD_GET_SYS		0x80
#define IT8951_CMD_READ_MEM		0x81
#define IT8951_CMD_WRITE_MEM		0x82
#define IT8951_CMD_DISPLAY_AREA		0x94
#define IT8951_CMD_SPI_ERASE		0x96
#define IT8951_CMD_SPI_READ		0x97
#define IT8951_CMD_SPI_WRITE		0x98
#define IT8951_CMD_LOAD_IMG_AREA	0xa2
#define IT8951_CMD_PMIC_CTRL		0xa3
#define IT8951_CMD_FAST_WRITE_MEM	0xa5
#define IT8951_CMD_AUTORESET		0xa7

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/*
 * This function converts a memory address or a buffer index into an argument
 * valid for the ITE device.
 */
static uint32_t memaddr_to_arg(struct it8951_device *dev, uint32_t memaddr)
{
	/*
	 * In case of a buffer index the argument parameter uses the following
	 * format: 0x80000000 | $index_number.
	 *
	 * Else in case of a memory address, it can be used directly.
	 *
	 * FIXME: index should be checked against dev->buf_num. But this field
	 * is not correctly defined by the IT8951 chip found on the Pathfinder
	 * board.
	 */
	if (memaddr < 3)
		memaddr |= (1 << 31);

	return memaddr;
}

static uint32_t supported_signatures[] =
{
	0x38393531, /* IT8951 */
};

static int it8951_check_signature(struct it8951_data *data)
{
	struct it8951_device *dev = data->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(supported_signatures); i++)
		if (supported_signatures[i] == dev->signature)
			return 0;

	fprintf(stderr,
		"Invalid device signature 0x%08x (maybe wrong /dev/sgX)\n",
		dev->signature);

	return ENODEV;
}

static int it8951_sg_get_sys(struct it8951_data *data)
{
	struct sg_io_hdr *sg_hdr = data->sg_hdr;
	struct it8951_device *dev;
	unsigned char sense[32];
	uint8_t cdb[16] = {
		[0] = IT8951_CMD_CUSTOMER,
		[1] = 0,
		[2] = 0x38,	/* signature[0] */
		[3] = 0x39,	/* signature[1] */
		[4] = 0x35,	/* signature[2] */
		[5] = 0x31,	/* signature[3] */
		[6] = IT8951_CMD_GET_SYS,
		[7] = 0,	/* version[0] */
		[8] = 0x01,	/* version[1] */
		[9] = 0,	/* version[2] */
		[10] = 0x02,	/* version[3] */
		[11] = 0,
		[12] = 0,
		[13] = 0,
		[14] = 0,
		[15] = 0,
	};

	info("sg: get system info\n");

	dev = calloc(1, sizeof(*dev));
	if (!dev) {
		fprintf(stderr, "Failed to calloc %ld bytes: %s\n",
			sizeof(*dev), strerror(errno));
		return ENOMEM;
	}

	/* Set sense buffer */
	sg_hdr->sbp = sense;
	sg_hdr->mx_sb_len = sizeof(sense);

	/* Set data buffer */
	sg_hdr->dxferp = dev;
	sg_hdr->dxfer_len = sizeof(*dev);

	/* Set CDB */
	sg_hdr->cmdp = cdb;
	sg_hdr->cmd_len = sizeof(cdb);
	sg_hdr->dxfer_direction = SG_DXFER_FROM_DEV;

	if (ioctl(data->fd, SG_IO, sg_hdr) == -1) {
		fprintf(stderr,
			"Get system info: SG_IO error: %s\n", strerror(errno));
		return errno;
	}

	dev->std_cmd_num = be32toh(dev->std_cmd_num);
	dev->ext_cmd_num = be32toh(dev->ext_cmd_num);
	dev->signature = be32toh(dev->signature);
	dev->version = be32toh(dev->version);
	dev->width = be32toh(dev->width);
	dev->height = be32toh(dev->height);
	dev->update_memaddr = be32toh(dev->update_memaddr);
	dev->memaddr = be32toh(dev->memaddr);
	dev->temp_seg_num = be32toh(dev->temp_seg_num);
	dev->mode = be32toh(dev->mode);
	dev->buf_num = be32toh(dev->buf_num);

	data->dev = dev;

	return 0;
}

void it8951_sg_info(struct it8951_data *data)
{
	struct it8951_device *dev = data->dev;

	fprintf(stdout, "Signature        : %08x\n", dev->signature);
	fprintf(stdout, "Version          : %08x\n", dev->version);
	fprintf(stdout, "Width            : %d\n", dev->width);
	fprintf(stdout, "Height           : %d\n", dev->height);
	fprintf(stdout, "Update address   : %08x\n", dev->update_memaddr);
	fprintf(stdout, "Memory address   : %08x\n", dev->memaddr);
	fprintf(stdout, "Mode             : %d\n", dev->mode);
	fprintf(stdout, "Number of buffer : %d\n", dev->buf_num);
}

struct sf_args_erase {
	uint32_t sfaddr;
	uint32_t size;
};

int it8951_sg_sf_erase(struct it8951_data *data, struct sf *sf,
		       uint32_t sfaddr, uint32_t size)
{
	struct sg_io_hdr *sg_hdr = data->sg_hdr;
	unsigned char sense[32];
	struct sf_args_erase args;
	int n_blocks, i;
	uint8_t cdb[16] = {
		[0] = IT8951_CMD_CUSTOMER,
		[1] = 0,
		[2] = 0,
		[3] = 0,
		[4] = 0,
		[5] = 0,
		[6] = IT8951_CMD_SPI_ERASE,
		[7] = 0,
		[8] = 0,
		[9] = 0,
		[10] = 0,
		[11] = 0,
		[12] = 0,
		[13] = 0,
		[14] = 0,
		[15] = 0,
	};

	if (!sf)
		return EINVAL;

	if (sfaddr % sf->block_size) {
		fprintf(stderr, "SPI flash erase: base address "
			"0x%08x is not aligned on block size (%d bytes)\n",
			sfaddr, sf->block_size);
		return EINVAL;
	}

	n_blocks = size / sf->block_size;
	if (size % sf->block_size)
		n_blocks++;

	info("sg: erase SPI flash @0x%08x (%d bytes)\n",
	     sfaddr, n_blocks * sf->block_size);

	/* Set sense buffer */
	sg_hdr->sbp = sense;
	sg_hdr->mx_sb_len = sizeof(sense);

	/* Data buffer */
	sg_hdr->dxferp = &args;
	sg_hdr->dxfer_len = sizeof(args);
	sg_hdr->dxfer_direction = SG_DXFER_TO_DEV;

	/* Set CDB */
	sg_hdr->cmdp = cdb;
	sg_hdr->cmd_len = sizeof(cdb);

	for (i = 0; i < n_blocks; i++) {
		debug("sg: erase block %d @0x%08x (%d bytes)\n",
		      i, sfaddr + i * sf->block_size, sf->block_size);
		args.sfaddr = htobe32(sfaddr + i * sf->block_size);
		args.size = htobe32(sf->block_size - 1);
		if (ioctl(data->fd, SG_IO, sg_hdr) == -1) {
			err("sg: SPI flash erase: SG_IO error: %s\n",
			    strerror(errno));
			return errno;
		}
	}


	return 0;
}

struct sf_args_data {
	uint32_t sfaddr;
	uint32_t memaddr;
	uint32_t size;
};

static int
it8951_sg_sf_data(struct it8951_data *data, uint32_t sfaddr,
		  uint32_t memaddr, uint32_t size, bool write)
{
	struct sg_io_hdr *sg_hdr = data->sg_hdr;
	unsigned char sense[32];
	struct sf_args_data args;
	uint8_t cdb[16] = {
		[0] = IT8951_CMD_CUSTOMER,
		[1] = 0,
		[2] = 0,
		[3] = 0,
		[4] = 0,
		[5] = 0,
		[6] = IT8951_CMD_SPI_READ,
		[7] = 0,
		[8] = 0,
		[9] = 0,
		[10] = 0,
		[11] = 0,
		[12] = 0,
		[13] = 0,
		[14] = 0,
		[15] = 0,
	};

	if (write)
		cdb[6] = IT8951_CMD_SPI_WRITE;

	if (write)
		info("sg: write from memory @0x%08x to SPI flash @0x%08x (%d bytes)\n",
		     memaddr, sfaddr, size);
	else
		info("sg: read from SPI flash @0x%08x to memory @0x%08x (%d bytes)\n",
		     sfaddr, memaddr, size);

	/* Set sense buffer */
	sg_hdr->sbp = sense;
	sg_hdr->mx_sb_len = sizeof(sense);

	/* Data buffer */
	sg_hdr->dxferp = &args;
	sg_hdr->dxfer_len = sizeof(args);
	sg_hdr->dxfer_direction = SG_DXFER_TO_DEV;

	/* Set CDB */
	sg_hdr->cmdp = cdb;
	sg_hdr->cmd_len = sizeof(cdb);

	args.memaddr = htobe32(memaddr);
	args.sfaddr = htobe32(sfaddr);
	args.size = htobe32(size);

	if (ioctl(data->fd, SG_IO, sg_hdr) == -1) {
		err("SPI flash read/write: SG_IO error: %s\n", strerror(errno));
		return errno;
	}

	return 0;
}

int it8951_sg_sf_write(struct it8951_data *data, struct sf *sf,
		       uint32_t sfaddr, uint32_t memaddr, uint32_t size)
{
	return it8951_sg_sf_data(data, sfaddr, memaddr, size, true);
}

int it8951_sg_sf_read(struct it8951_data *data, struct sf *sf,
		      uint32_t sfaddr, uint32_t memaddr, uint32_t size)
{
	return it8951_sg_sf_data(data, sfaddr, memaddr, size, false);
}

/*
 * According to the IT8951_USB_ProgrammingGuide_v.0.4_20161114.pdf document,
 * the control PMIC command don't return any data. But from experimentation we
 * noticed there are.
 */
struct pmic_regs {
	int16_t vcom;
	uint8_t set_vcom;
	uint8_t set_pwr;
	uint8_t pwr;
	uint8_t unused[11];
} __attribute__((packed));

int it8951_sg_pmic(struct it8951_data *data, uint16_t *vcom, uint8_t *pwr)
{
	struct sg_io_hdr *sg_hdr = data->sg_hdr;
	uint16_t *vcom_ptr;
	unsigned char sense[32];
	struct pmic_regs pmic;
	int i;
	uint8_t cdb[16] = {
		[0] = IT8951_CMD_CUSTOMER,
		[1] = 0,
		[2] = 0,
		[3] = 0,
		[4] = 0,
		[5] = 0,
		[6] = IT8951_CMD_PMIC_CTRL,
		[7] = 0,	/* Vcom value [15:8] */
		[8] = 0,	/* Vcom value [7:0] */
		[9] = 0,	/* Set Vcom 0=no 1=yes */
		[10] = 0,	/* Set power 0=no 1=yes */
		[11] = 0,	/* Power value 0=off 1=on */
		[12] = 0,
		[13] = 0,
		[14] = 0,
		[15] = 0,
	};

	info("sg: PMIC control\n");

	if (pwr) {
		cdb[10] = 1;
		cdb[11] = *pwr;
	}
	if (vcom) {
		cdb[9] = 1;
		vcom_ptr = (uint16_t *) &cdb[7];
		*vcom_ptr = htobe16(*vcom);
	}

	/* Set sense buffer */
	sg_hdr->sbp = sense;
	sg_hdr->mx_sb_len = sizeof(sense);

	/* Data buffer */
	sg_hdr->dxferp = &pmic;
	sg_hdr->dxfer_len = sizeof(pmic);
	sg_hdr->dxfer_direction = SG_DXFER_FROM_DEV;

	/* Set CDB */
	sg_hdr->cmdp = cdb;
	sg_hdr->cmd_len = sizeof(cdb);

	debug("CDB:");
	for (i = 0; i < sizeof(cdb); i++)
		print_log(DEBUG, " %02x", cdb[i]);
	print_log(DEBUG, "\n");

	if (ioctl(data->fd, SG_IO, sg_hdr) == -1) {
		err("PMIC control: SG_IO error: %s\n", strerror(errno));
		return errno;
	}

	if (pwr)
		fprintf(stdout, "PMIC control - power:%s set:%s\n",
			pmic.pwr ? "on" : "off", pmic.set_pwr ? "yes" : "no");
	else
		fprintf(stdout, "PMIC control - VCom:%hdmV set:%s\n",
			be16toh(pmic.vcom), pmic.set_vcom ? "yes" : "no");

	return 0;
}

int it8951_sg_read_mem(struct it8951_data *data, uint32_t memaddr,
		       char *buf, size_t size)
{
	struct sg_io_hdr *sg_hdr = data->sg_hdr;
	int read = 0;
	unsigned char sense[32];
	uint8_t cdb[16] = {
		[0] = IT8951_CMD_CUSTOMER,
		[1] = 0,
		[2] = 0,	/* memory address [31:24] */
		[3] = 0,	/* memory address [23:16] */
		[4] = 0,	/* memory address [16:8] */
		[5] = 0,	/* memory address [7:0] */
		[6] = IT8951_CMD_READ_MEM,
		[7] = 0,	/* length[15-8] */
		[8] = 0,	/* length[7-0] */
		[9] = 0,
		[10] = 0,
		[11] = 0,
		[12] = 0,
		[13] = 0,
		[14] = 0,
		[15] = 0,
	};

	info("sg: read from memory @0x%08x (%ld bytes)\n", memaddr, size);

	/* Set sense buffer */
	sg_hdr->sbp = sense;
	sg_hdr->mx_sb_len = sizeof(sense);

	/* Set CDB */
	sg_hdr->cmdp = cdb;
	sg_hdr->cmd_len = sizeof(cdb);
	sg_hdr->dxfer_direction = SG_DXFER_FROM_DEV;

	while (read < size) {
		int read_size, i;
		uint32_t *addr;
		uint16_t *len;

		/*
		 * For the read command, a short is used to encode the transfer
		 * size. So the limit is 2^16 - 1 bytes.
		 *
		 * FIXME: Is there some kind of limit from the sg layer ?
		 */
		if ((size - read) < (1 << 16) - 1)
			read_size = size - read;
		else
			read_size = (1 << 16) - 1;

		/* Set data buffer */
		sg_hdr->dxferp = buf + read;
		sg_hdr->dxfer_len = read_size;

		/*
		 * Complete CDB with the source memory address (device)
		 * and the buffer length
		 */
		addr = (uint32_t *) &cdb[2];
		*addr = htobe32(memaddr + read);

		len = (uint16_t *) &cdb[7];
		*len = htobe16(read_size);

		debug("sg: read @%08x (%d bytes)\n", memaddr + read, read_size);
		debug("sg: CDB:");
		for (i = 0; i < sizeof(cdb); i++)
			print_log(DEBUG, " %02x", cdb[i]);
		print_log(DEBUG, "\n");

		if (ioctl(data->fd, SG_IO, sg_hdr) == -1) {
			err("Read memory: SG_IO error: %s\n", strerror(errno));
			return errno;
		}

		read += read_size;
	}

	return 0;
}

int it8951_sg_write_mem(struct it8951_data *data, uint32_t memaddr,
			const char *buf, size_t size, bool fast)
{
	struct sg_io_hdr *sg_hdr = data->sg_hdr;
	int written = 0;
	unsigned char sense[32];
	uint8_t cdb[16] = {
		[0] = IT8951_CMD_CUSTOMER,
		[1] = 0,
		[2] = 0,	/* memory address [31:24] */
		[3] = 0,	/* memory address [23:16] */
		[4] = 0,	/* memory address [16:8] */
		[5] = 0,	/* memory address [7:0] */
		[6] = IT8951_CMD_WRITE_MEM,
		[7] = 0,	/* length[15-8] */
		[8] = 0,	/* length[7-0] */
		[9] = 0,
		[10] = 0,
		[11] = 0,
		[12] = 0,
		[13] = 0,
		[14] = 0,
		[15] = 0,
	};

	info("sg: write to memory @0x%08x (%ld bytes, fast=%d)\n",
	     memaddr, size, fast);

	if (fast)
		cdb[6] = IT8951_CMD_FAST_WRITE_MEM;

	/* Set sense buffer */
	sg_hdr->sbp = sense;
	sg_hdr->mx_sb_len = sizeof(sense);

	/* Set CDB */
	sg_hdr->cmdp = cdb;
	sg_hdr->cmd_len = sizeof(cdb);
	sg_hdr->dxfer_direction = SG_DXFER_TO_DEV;

	while (written < size) {
		int write_size, i;
		uint32_t *addr;
		uint16_t *len;

		/*
		 * For the write command, a short is used to encode the transfer
		 * size. So the limit is 2^16 - 1 bytes.
		 *
		 * FIXME: Is there some kind of limit from the sg layer ?
		 */
		if (size - written < (1 << 16) - 1)
			write_size = size - written;
		else
			write_size = (1 << 16) - 1;

		/* Set data buffer */
		sg_hdr->dxferp = (char *) buf + written;
		sg_hdr->dxfer_len = write_size;

		/*
		 * Complete CDB with the destination memory address
		 * and the buffer length
		 */
		addr = (uint32_t *) &cdb[2];
		*addr = htobe32(memaddr + written);

		len = (uint16_t *) &cdb[7];
		*len = htobe16(write_size);

		debug("sg: write @%08x (%d bytes)\n", memaddr + written, write_size);
		debug("sg: CDB:");
		for (i = 0; i < sizeof(cdb); i++)
			print_log(DEBUG, " %02x", cdb[i]);
		print_log(DEBUG, "\n");

		if (ioctl(data->fd, SG_IO, sg_hdr) == -1) {
			err("Write memory: SG_IO error: %s\n", strerror(errno));
			return errno;
		}

		written += write_size;
	}

	return 0;
}

/*
 * This fonction build a valid screen zone based on the user input, the image
 * size and the screen dimensions.
 */
static int sanitize_zone(struct zone *zone, const struct zone *user,
			 struct it8951_device *dev, struct image *img)
{
	if (!zone || !dev)
		return EINVAL;

	memset(zone, 0, sizeof(*zone));

	if (user) {
		memcpy(zone, user, sizeof(*zone));
		info("Zone (user args): x=%d y=%d width=%d height=%d\n",
		     zone->x, zone->y, zone->width, zone->height);
	}
	/*
	 * - Resize the zone if it exceeds the img dimension.
	 * - If the zone is not defined (WxH set to 0x0), then use the img
	 *   dimensions.
	 */
	if (img) {
		if (!zone->width || zone->width > img->width)
			zone->width = img->width;
		if (!zone->height || zone->width > img->height)
			zone->height = img->height;
	}
	/*
	 * - Resize the zone if it exceeds the screen dimension.
	 * - If the zone is not defined (WxH set to 0x0), then use the img
	 *   dimensions.
	 */
	if (!zone->width || (zone->x + zone->width) > dev->width)
		zone->width = dev->width - zone->x;
	if (!zone->height || (zone->y + zone->height) > dev->height)
		zone->height = dev->height - zone->y;

	info("Zone (sanitized): x=%d y=%d width=%d height=%d\n",
	     zone->x, zone->y, zone->width, zone->height);

	return 0;
}

struct load_area_args {
	uint32_t memaddr;
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
} __attribute__((packed));

int it8951_sg_load_area(struct it8951_data *data, uint32_t memaddr,
			struct image *img, struct zone *u_zone)
{
	int err, i;
	struct it8951_device *dev = data->dev;
	struct sg_io_hdr *sg_hdr = data->sg_hdr;
	struct zone zone;
	struct load_area_args args;
	char *buf;
	unsigned char sense[32];
	uint8_t cdb[16] = {
		[0] = IT8951_CMD_CUSTOMER,
		[1] = 0,
		[2] = 0,
		[3] = 0,
		[4] = 0,
		[5] = 0,
		[6] = IT8951_CMD_LOAD_IMG_AREA,
		[7] = 0,
		[8] = 0,
		[9] = 0,
		[10] = 0,
		[11] = 0,
		[12] = 0,
		[13] = 0,
		[14] = 0,
		[15] = 0,
	};

	info("sg: load area\n");

	if (!img) {
		err("Error: image is missing\n");
		return EINVAL;
	}

	err = sanitize_zone(&zone, u_zone, dev, img);
	if (err)
		return err;

	memaddr = memaddr_to_arg(dev, memaddr);

	/*
	 * Set the load area arguments
	 */
	memset(&args, 0, sizeof(args));
	args.memaddr = htobe32(memaddr);
	args.x = htobe32(zone.x);
	args.y = htobe32(zone.y);
	args.width = htobe32(zone.width);
	args.height = htobe32(zone.height);

	debug("Memory address: %08x\n", memaddr);
	debug("Data size: %d\n", zone.width * zone.height);
	debug("DATA (without image):");
	for (i = 0; i < sizeof(args); i++)
		print_log(DEBUG, " %02x", ((char *) &args)[i]);
	print_log(DEBUG, "\n");

	buf = malloc(sizeof(args) + zone.width * zone.height);
	if (!buf) {
		err("Failed to malloc %ld bytes: %s\n",
		    sizeof(args) + zone.width * zone.height, strerror(errno));
		return ENOMEM;
	}
	memcpy(buf, &args, sizeof(args));
	memcpy(buf + sizeof(args), img->buf, zone.width * zone.height);

	/* Set sense buffer */
	sg_hdr->sbp = sense;
	sg_hdr->mx_sb_len = sizeof(sense);

	/* Set data buffer */
	sg_hdr->dxferp = buf;
	sg_hdr->dxfer_len = sizeof(args) + zone.width * zone.height;

	/* Set CDB */
	sg_hdr->cmdp = cdb;
	sg_hdr->cmd_len = sizeof(cdb);
	sg_hdr->dxfer_direction = SG_DXFER_TO_DEV;

	if (ioctl(data->fd, SG_IO, sg_hdr) == -1) {
		err("Load area: SG_IO error: %s\n", strerror(errno));
		return errno;
	}

	return 0;
}

struct display_area_args {
	uint32_t memaddr;
	uint32_t mode;
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
	uint32_t en_ready;
} __attribute__((packed));

int it8951_sg_display_area(struct it8951_data *data, uint32_t memaddr,
			   uint32_t mode, struct zone *u_zone)
{
	int i, err;
	struct it8951_device *dev = data->dev;
	struct sg_io_hdr *sg_hdr = data->sg_hdr;
	struct zone zone;
	struct display_area_args args;
	unsigned char sense[32];
	uint8_t cdb[16] = {
		[0] = IT8951_CMD_CUSTOMER,
		[1] = 0,
		[2] = 0,
		[3] = 0,
		[4] = 0,
		[5] = 0,
		[6] = IT8951_CMD_DISPLAY_AREA,
		[7] = 0,
		[8] = 0,
		[9] = 0,
		[10] = 0,
		[11] = 0,
		[12] = 0,
		[13] = 0,
		[14] = 0,
		[15] = 0,
	};

	info("sg: display area\n");

	memaddr = memaddr_to_arg(dev, memaddr);

	err = sanitize_zone(&zone, u_zone, dev, NULL);
	if (err)
		return err;

	/*
	 * Set the display area arguments
	 */
	memset(&args, 0, sizeof(args));
	args.memaddr = htobe32(memaddr);
	args.mode = htobe32(mode);
	args.en_ready = htobe32(1);
	args.x = htobe32(zone.x);
	args.y = htobe32(zone.y);
	args.width = htobe32(zone.width);
	args.height = htobe32(zone.height);

	debug("Memory address: %08x\n", memaddr);
	debug("Mode: %d\n", mode);
	debug("DATA:");
	for (i = 0; i < sizeof(args); i++)
		print_log(DEBUG, " %02x", ((char *) &args)[i]);
	print_log(DEBUG, "\n");

	/* Set sense buffer */
	sg_hdr->sbp = sense;
	sg_hdr->mx_sb_len = sizeof(sense);

	/* Set data buffer */
	sg_hdr->dxferp = &args;
	sg_hdr->dxfer_len = sizeof(args);

	/* Set CDB */
	sg_hdr->cmdp = cdb;
	sg_hdr->cmd_len = sizeof(cdb);
	sg_hdr->dxfer_direction = SG_DXFER_TO_DEV;

	if (ioctl(data->fd, SG_IO, sg_hdr) == -1) {
		err("Display area: SG_IO error: %s\n", strerror(errno));
		return errno;
	}

	return 0;
}

int it8951_sg_open(struct it8951_data **data, const char *devname)
{
	int err;
	struct sg_io_hdr *sg_hdr;

	info("Opening ITE device: %s\n", devname);

	*data = calloc(1, sizeof(struct it8951_data));
	if (!*data) {
		err("Failed to calloc %ld bytes: %s\n",
		    sizeof(struct it8951_data), strerror(errno));
		return ENOMEM;
	}

	err = open(devname, O_RDWR);
	if (err == -1) {
		err = errno;
		err("Failed to open ITE device [%s]: %s\n",
		    devname, strerror(errno));
		goto exit_free_data;
	}
	(*data)->fd = err;

	sg_hdr = calloc(1, sizeof(*sg_hdr));
	if (!sg_hdr) {
		err("Failed to calloc %ld bytes: %s\n",
		    sizeof(*sg_hdr), strerror(errno));
		err = ENOMEM;
		goto exit_close;
	}
	sg_hdr->interface_id = 'S';
	sg_hdr->flags = SG_FLAG_LUN_INHIBIT;
	(*data)->sg_hdr = sg_hdr;

	err = it8951_sg_get_sys(*data);
	if (err)
		goto exit_free_sg_hdr;

	err = it8951_check_signature(*data);
	if (err)
		goto exit_free_sg_hdr;

	return 0;

exit_free_sg_hdr:
	free((*data)->sg_hdr);
exit_close:
	close((*data)->fd);
exit_free_data:
	free(*data);
	return err;
}

void it8951_sg_close(struct it8951_data *data)
{
	free(data->dev);
	free(data->sg_hdr);
	close(data->fd);
	free(data);
}
