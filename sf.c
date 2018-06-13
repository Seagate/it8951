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
#include <errno.h>

#include "debug.h"
#include "sf.h"
#include "sg.h"

/* TODO: allow user to set flash characteristics with options. */
static struct sf sf = {
	.block_size = 64 * 1024,
	.n_blocks = 64,
	.sector_size = 4 * 1024,
};

/*
 * Align a flash address with the previous erase block.
 */
uint32_t sf_block_align_prev(uint32_t addr)
{
	return addr & ~(sf.block_size - 1);
}

/*
 * Align a flash address with the next erase block.
 */
uint32_t sf_block_align_next(uint32_t addr)
{
	uint32_t baddr;

	baddr = addr & ~(sf.block_size - 1);
	if (baddr == addr)
		return addr;

	return baddr + sf.block_size;
}

/*
 * Erase the SPI flash at a given address and for a given size.
 */
int sf_erase(struct it8951_data *data, uint32_t memaddr,
	     uint32_t addr, uint32_t size)
{
	return it8951_sg_sf_erase(data, &sf, addr, size);
}

/*
 * Read SPI flash from a given address into a buffer.
 */
int sf_read(struct it8951_data *data, uint32_t memaddr,
	    uint32_t addr, uint32_t count, char *buf)
{
	uint32_t membuf_size = data->dev->width * data->dev->height;
	int read = 0;
	int ret;

	info("sf: reading SPI flash @0x%08x (%d bytes)\n", addr, count);

	if (addr + count > SF_SIZE) {
		err("I/O beyond the end of the device\n");
		return EINVAL;
	}

	do {
		uint32_t size = membuf_size;

		if (count - read < size)
			size = count - read;

		ret = it8951_sg_sf_read(data, &sf, addr + read, memaddr, size);
		if (ret)
			return ret;

		ret = it8951_sg_read_mem(data, memaddr, buf + read, size);
		if (ret)
			return ret;

		read += size;
	} while (read < count);

	return 0;
}

/*
 * Compare a flash section with a reference buffer.
 */
int sf_verify(struct it8951_data *data, uint32_t memaddr,
	      uint32_t addr, uint32_t size, const char *ref)
{
	char *buf;
	int ret;

	info("sf: verifying SPI flash @0x%08x (%d bytes)\n", addr, size);

	buf = malloc(size);
	if (!buf) {
		err("Failed to malloc %d bytes: %s\n", size, strerror(errno));
		return ENOMEM;
	}

	ret = sf_read(data, memaddr, addr, size, buf);
	if (ret)
		goto exit_free;

	ret = memcmp(buf, ref, size);
	if (ret) {
		ret = EIO;
		err("Corruption detected on SPI flash @0x%08x (%d bytes)\n",
		    addr, size);
		goto exit_free;
	}

	info("sf: verification successful\n");

exit_free:
	free(buf);
	return ret;
}

/*
 * Write a buffer at a given address into SPI flash. The destination address
 * and the buffer size must be both aligned with the flash "erase block" size.
 */
static int sf_write_aligned(struct it8951_data *data, uint32_t memaddr,
			    const char *buf, uint32_t count,
			    uint32_t addr, bool verify)
{
	int ret;
	uint32_t membuf_size = data->dev->width * data->dev->height;
	int written = 0;

	ret = it8951_sg_sf_erase(data, &sf, addr, count);
	if (ret)
		return ret;

	do {
		uint32_t size = membuf_size;

		if (count - written < membuf_size)
			size = count - written;

		ret = it8951_sg_write_mem(data, memaddr,
					  buf + written, size, false);
		if (ret)
			return ret;

		ret = it8951_sg_sf_write(data, &sf,
					 addr + written, memaddr, size);
		if (ret)
			return ret;

		written += size;
	} while (written < count);

	if (verify)
		ret = sf_verify(data, memaddr, addr, count, buf);

	return ret;
}

/*
 * Write a buffer at a given address into SPI flash.
 */
int sf_write(struct it8951_data *data, uint32_t memaddr,
             const char *buf, uint32_t count, uint32_t addr, bool verify)
{
	uint32_t start, end, offset;
	char *buf_align;
	int size, ret;

	info("sf: writing SPI flash @0x%08x (%d bytes)\n", addr, count);

	if (addr + count > SF_SIZE) {
		err("I/O beyond the end of the device\n");
		return EINVAL;
	}

	start = sf_block_align_prev(addr);
	end = sf_block_align_next(addr + count);

	if (start == addr && end == (addr + count))
		return sf_write_aligned(data, memaddr, buf, count, addr, verify);

	offset = addr - start;
	size = end - start;

	info("sf: aligning I/O on block size: 0x%08x-0x%08x (%d bytes)\n",
	     start, end, size);

	buf_align = malloc(size);
	if (!buf_align) {
		err("Failed to malloc %d bytes: %s\n", size, strerror(errno));
		return ENOMEM;
	}

	ret = sf_read(data, memaddr, start, size, buf_align);
	if (ret)
		goto exit_free;

	memcpy(buf_align + offset, buf, count);

	ret = sf_write_aligned(data, memaddr,
			       buf_align, size, start, verify);

exit_free:
	free(buf_align);
	return ret;
}
