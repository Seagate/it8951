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

#include "sg.h"
#include "file.h"
#include "fw.h"
#include "image.h"

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
	fprintf(stdout, "Usage : it8951_fw [OPTIONS] [DEVICE] [COMMANDS]\n");
	fprintf(stdout, "\nOptions:\n");
#ifdef HAVE_GETOPT_LONG
	fprintf(stdout, "    -h, --help              display this help\n");
	fprintf(stdout, "    -m, --memaddr           memory address or buffer index\n");
	fprintf(stdout, "    -v, --verbose           enable verbose messages\n");
#else
	fprintf(stdout, "    -h                      display this help\n");
	fprintf(stdout, "    -m                      memory address or buffer index\n");
	fprintf(stdout, "    -v                      enable verbose messages\n");
#endif
	fprintf(stdout, "\nDevice: SCSI generic device name (e.g. /dev/sg2)\n");
	fprintf(stdout, "\nCommands:\n");
	fprintf(stdout, "    enable_bs index         Set active bootscreen image\n\n");
	fprintf(stdout, "    info                    print firmware version and flash layout\n\n");
	fprintf(stdout, "    write_bs file index     write a boot screen image at the given index in SPI\n");
	fprintf(stdout, "                            flash. The maximum index value depends on the flash\n");
	fprintf(stdout, "                            size. You can use the print_layout command to find\n");
	fprintf(stdout, "                            out how many indexes are available\n\n");
	fprintf(stdout, "    write_fw file           write a firmware image in SPI flash\n\n");
}

int verbose = 0;

/*
 * Get firmware from file and write it into flash.
 */
static int write_fw_cmd(struct it8951_data *data, uint32_t memaddr,
			const char *fname)
{
	char *fw;
	size_t fw_size;
	int ret;

	fprintf(stdout, "Reading firmware from file %s\n", fname);

	ret = read_buf_from_file(fname, &fw, &fw_size);
	if (ret < 0)
		return ret;

	ret = fw_write_img(data, memaddr, fw, fw_size);

	free(fw);
	return ret;
}

/*
 * Read boot screen image from file and write it into flash.
 */
static int write_bs_cmd(struct it8951_data *data, uint32_t memaddr,
			struct fw_info *fw_info, const char *fname, int index)
{
	struct image *img;
	int ret;

	img = load_image(fname);
	if (!img)
		return EINVAL;

	ret = fw_write_bs(data, memaddr, fw_info, img->buf,
			  img->width * img->height, index);

	free(img);
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	uint32_t memaddr = 0;
	struct it8951_data *data;
	struct fw_info *fw_info = NULL;
	const char *dev;
	const char *cmd;
	const char *fname;
	int index;
	int num_args;
	int opt;
#ifdef HAVE_GETOPT_LONG
	int option_index = 0;

	while ((opt = getopt_long(argc, argv, short_options, long_options,
					&option_index)) != EOF)
#else
	while ((opt = getopt(argc, argv, short_options)) != EOF)
#endif
	{
		char *endptr = NULL;

		switch (opt) {
		case 'h': /* --help */
			usage();
			return 0;
		case 'm': /* --memaddr */
			memaddr = strtoul(optarg, &endptr, 0);
			if (optarg == endptr || errno) {
				fprintf(stderr,
					"Invalid address argument: %s\n",
					optarg);
				return EINVAL;
			}
			break;
		case 'v': /* --verbose */
			verbose++;
			break;
		default:
			usage();
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

	if (!strcmp(cmd, "write_fw") && num_args == 1) {
		fname = argv[optind];
		ret = write_fw_cmd(data, memaddr, fname);
		goto exit_sg_close;
	}

	/* Retrieve firmare layout information (needed for all the
	 * commands below). */
	ret = fw_get_info(data, memaddr, &fw_info);
	if (ret)
		goto exit_sg_close;

	if (!strcmp(cmd, "enable_bs") && num_args == 1) {
		index = atoi(argv[optind]);
		ret = fw_enable_bs(data, memaddr, fw_info, index);
		goto exit_fw_put;
	}

	if (!strcmp(cmd, "info") && !num_args) {
		fw_print_info(fw_info);
		ret = 0;
		goto exit_fw_put;
	}

	if (!strcmp(cmd, "write_bs") && num_args == 2) {
		fname = argv[optind++];
		index = atoi(argv[optind]);
		ret = write_bs_cmd(data, memaddr, fw_info, fname, index);
		goto exit_fw_put;
	}

	fprintf(stderr, "Invalid command: %s", cmd);
	for (opt = optind; opt < argc; opt++)
		fprintf(stderr, " %s", argv[opt]);
	fprintf(stderr, "\n");

	ret = EINVAL;

exit_fw_put:
	if (fw_info)
		fw_put_info(fw_info);
exit_sg_close:
	it8951_sg_close(data);

	return ret;
}
