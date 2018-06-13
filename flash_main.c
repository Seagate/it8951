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

#include "common.h"
#include "file.h"
#include "sg.h"
#include "sf.h"

#include <getopt.h>

#ifdef HAVE_GETOPT_LONG
static const struct option long_options[] =
{
	{"help", 0, 0, 'h'},
	{"memaddr", 1, 0, 'm'},
	{"verbose", 0, 0, 'v'},
	{0, 0, 0, 0}
};
#endif

static const char *short_options = "hm:v";

static void usage(void)
{
	fprintf(stdout, "Usage : it8951_flash [OPTIONS] [DEVICE] [COMMANDS]\n");
	fprintf(stdout, "\nOptions:\n");
#ifdef HAVE_GETOPT_LONG
	fprintf(stdout, "    -h, --help         display this help\n");
	fprintf(stdout, "    -m, --memaddr      memory address or buffer index\n");
	fprintf(stdout, "    -v, --verbose      enable verbose messages\n");
#else
	fprintf(stdout, "    -h                 display this help\n");
	fprintf(stdout, "    -m                 memory address or buffer index\n");
	fprintf(stdout, "    -v                 enable verbose messages\n");
#endif
	fprintf(stdout, "\nDevice: SCSI generic device name (e.g. /dev/sg2)\n");
	fprintf(stdout, "\nCommands:\n");
	fprintf(stdout, "    erase  addr size            Erase flash size at the given address\n\n");
	fprintf(stdout, "    read   addr file [size]     copy data from a flash address to a file\n");
	fprintf(stdout, "                                (size=all if omitted)\n\n");
	fprintf(stdout, "    write  file addr [size]     copy data from a file to a flash address\n");
	fprintf(stdout, "                                (size=all if omitted)\n\n");
}

int verbose = 0;

/*
 * Read flash content (at the given address and for the given size) and save it
 * in a file.
 */
static int read_flash_cmd(struct it8951_data *data, uint32_t memaddr,
			  uint32_t faddr, const char *fname, uint32_t size)
{
	char *buf;
	int ret;

	if (!size || size > (SF_SIZE - faddr))
		size = SF_SIZE - faddr;

	buf = malloc(size);
	if (!buf) {
		fprintf(stderr, "Failed to malloc %d bytes: %s\n",
			size, strerror(errno));
		return ENOMEM;
	}

	fprintf(stdout,
		"Copying %d bytes from flash address %08x into file %s\n",
		size, faddr, fname);

	ret = sf_read(data, memaddr, faddr, size, buf);
	if (ret)
		goto exit_free;

	ret = write_buf_to_file(fname, buf, size);

exit_free:
	free(buf);
	return ret;
}

/*
 * Write file content into flash (at the given address).
 */
static int write_flash_cmd(struct it8951_data *data, uint32_t memaddr,
			   const char *fname, uint32_t faddr, uint32_t size)
{
	int ret;
	char *buf;
	size_t fsize;

	ret = read_buf_from_file(fname, &buf, &fsize);
	if (ret < 0)
		return ret;

	if (!size || fsize < size)
		size = fsize;
	if (size > SF_SIZE - faddr)
		size = SF_SIZE - faddr;

	fprintf(stdout,
		"Copying %d bytes from file %s to flash address %08x\n",
		size, fname, faddr);

	ret = sf_write(data, memaddr, buf, size, 0, true);

	free(buf);
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int num_args;
	uint32_t memaddr = 0;
	uint32_t faddr = 0;
	uint32_t size = 0;
	struct it8951_data *data;
	const char *dev;
	const char *cmd;
	const char *fname;
	int opt;
#ifdef HAVE_GETOPT_LONG
	int option_index = 0;

	while ((opt = getopt_long(argc, argv, short_options, long_options,
					&option_index)) != EOF)
#else
	while ((opt = getopt(argc, argv, short_options)) != EOF)
#endif
	{
		switch (opt) {
		case 'h': /* --help */
			usage();
			return 0;
		case 'm': /* --memaddr */
			ret = string_to_addr(optarg, &memaddr);
			if (ret)
				return EINVAL;
			break;
		case 'v': /* --verbose */
			verbose++;
			break;
		default:
			fprintf(stderr, "Invalid option [-%c]\n", opt);
			return EINVAL;
		}
	}

	/* Device argument. */
	dev = argv[optind++];
	if (!dev) {
		fprintf(stderr, "Missing device name argument\n");
		return EINVAL;
	}
	/* Command argument. */
	cmd = argv[optind++];
	if (!cmd) {
		fprintf(stderr, "Missing command argument\n");
		return EINVAL;
	}
	num_args = argc - optind;

	/* Open and initialize ITE controller. */
	ret = it8951_sg_open(&data, dev);
	if (ret)
		return ret;

	if (!memaddr)
		memaddr = data->dev->memaddr;

	if (!strcmp(cmd, "erase") && num_args == 2) {
		ret = string_to_addr(argv[optind++], &faddr);
		if (ret) {
			ret = EINVAL;
			goto exit_close;
		}
		size = atoi(argv[optind]);
		ret = sf_erase(data, memaddr, faddr, size);
		goto exit_close;
	}

	if (!strcmp(cmd, "read") && (num_args == 2 || num_args == 3)) {
		ret = string_to_addr(argv[optind++], &faddr);
		if (ret) {
			ret = EINVAL;
			goto exit_close;
		}
		fname = argv[optind++];
		if (num_args == 3)
			size = atoi(argv[optind]);
		ret = read_flash_cmd(data, memaddr, faddr, fname, size);
		goto exit_close;
	}

	if (!strcmp(cmd, "write") && (num_args == 2 || num_args == 3)) {
		fname = argv[optind++];
		ret = string_to_addr(argv[optind++], &faddr);
		if (ret) {
			ret = EINVAL;
			goto exit_close;
		}
		if (num_args == 3)
			size = atoi(argv[optind]);
		ret = write_flash_cmd(data, memaddr, fname, faddr, size);
		goto exit_close;
	}

	fprintf(stderr, "Invalid command: %s", cmd);
	for (opt = optind; opt < argc; opt++)
		fprintf(stderr, " %s", argv[opt]);
	fprintf(stderr, "\n");

	ret = EINVAL;
exit_close:
	it8951_sg_close(data);

	return ret;
}
