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
#include "image.h"
#include "file.h"

#define _GNU_SOURCE
#include <getopt.h>

#ifdef HAVE_GETOPT_LONG
static const struct option long_options[] =
{
	{"help", 0, 0, 'h'},
	{"memaddr", 1, 0, 'm'},
	{"verbose", 0, 0, 'v'},
	{"waveform", 1, 0, 'w'},
	{0, 0, 0, 0}
};
#endif

static const char *short_options = "hm:vw:";

static void usage(void)
{
	fprintf(stdout, "Usage : it8951_cmd [OPTIONS] [DEVICE] [COMMANDS]\n");
	fprintf(stdout, "\nOptions:\n");
#ifdef HAVE_GETOPT_LONG
	fprintf(stdout, "    -h, --help          display this help\n");
	fprintf(stdout, "    -m, --memaddr       memory address or buffer index\n");
	fprintf(stdout, "    -v, --verbose       enable verbose messages\n");
	fprintf(stdout, "    -w, --waveform      set waveform mode to use\n");
#else
	fprintf(stdout, "    -h                  display this help\n");
	fprintf(stdout, "    -m                  memory address or buffer index\n");
	fprintf(stdout, "    -v                  enable verbose messages\n");
	fprintf(stdout, "    -w                  set waveform mode to use\n");
#endif
	fprintf(stdout, "\nDevice: SCSI generic device name (e.g. /dev/sg2)\n");
	fprintf(stdout, "\nCommands:\n");
	fprintf(stdout, "    clear   [XxY[xWxH]] clear screen\n");
	fprintf(stdout, "    info                display device information\n");
	fprintf(stdout, "    power   on|off      Set power state\n");
	fprintf(stdout, "    vcom    [mV]        Get or set Vcom value (in mV)\n");
	fprintf(stdout, "    load    [XxY[xWxH]] load image into a memory area\n");
	fprintf(stdout, "    write   file|WxHxC  write file (or monochrome image) into memory\n");
	fprintf(stdout, "    fwrite  file|WxHxC  fast write file (or monochrome image) into memory\n");
	fprintf(stdout, "    read    file        read memory and store it into file\n");
	fprintf(stdout, "    display [XxY[xWxH]] display a memory area\n");
}

int verbose = 0;

/*
 * Get a screen zone from the user arguments. If any given, returns a zone with
 * all the coordinates set to zero.
 */
static bool get_zone_from_arg(const char *arg, struct zone *zone)
{
	int match;

	memset(zone, 0, sizeof(*zone));
	if (!arg)
		return false;

	match=sscanf(arg, "%dx%dx%dx%d",
		     &zone->x, &zone->y, &zone->width, &zone->height);
	if (match == 2 || match == 4)
		return true;

	memset(zone, 0, sizeof(*zone));

	return false;
}

/*
 * Wrappers for SG commands.
 */

static int do_write_mem_cmd(struct it8951_data *data, uint32_t memaddr,
			    bool fast, const char *arg_img)
{
	struct image *img;
	int ret;

	if (!arg_img) {
		fprintf(stderr, "Missing image argument for write command\n");
		return EINVAL;
	}
	/* Consume image argument. */
	optind++;

	img = load_image(arg_img);
	if (!img)
		return EINVAL;

	ret = it8951_sg_write_mem(data, memaddr,
				  img->buf, img->width * img->height, fast);
	free(img);

	return ret;
}

static int do_read_mem_cmd(struct it8951_data *data,
			   uint32_t memaddr, const char *arg_fname)
{
	uint32_t size;
	char *buf;
	int ret;

	if (!arg_fname) {
		fprintf(stderr, "Missing filename argument for read command\n");
		return EINVAL;
	}
	/* Consume filename argument. */
	optind++;

	/*
	 * FIXME: size is set to the screen size (width x height x pixel size).
	 *        But a user may want to configure it.
	 */
	size = data->dev->width * data->dev->height;
	buf = malloc(size);
	if (!buf) {
		fprintf(stderr, "Failed to malloc %d bytes: %s\n",
			size, strerror(errno));
		return ENOMEM;
	}

	ret = it8951_sg_read_mem(data, memaddr, buf, size);
	if (ret)
		goto exit_free;

	ret = write_buf_to_file(arg_fname, buf, size);
exit_free:
	free(buf);
	return ret;
}

static int do_load_area_cmd(struct it8951_data *data, uint32_t memaddr,
			    const char *arg_img, const char *arg_zone)
{
	struct image *img;
	struct zone zone;
	int ret;

	if (!arg_img) {
		fprintf(stderr, "Missing image argument for load_area command\n");
		return EINVAL;
	}
	/* Consume image argument. */
	optind++;

	img = load_image(arg_img);
	if (!img)
		return EINVAL;

	/* Get area specified by the user. */
	if (get_zone_from_arg(arg_zone, &zone))
		optind++; /* Consume zone argument. */

	ret = it8951_sg_load_area(data, memaddr, img, &zone);

	free(img);

	return ret;
}

static int do_display_area_cmd(struct it8951_data *data, uint32_t memaddr,
			       uint32_t mode, const char *arg_zone)
{
	struct zone zone;

	/* Get rea specified by the user. */
	if (get_zone_from_arg(arg_zone, &zone))
		optind++; /* Consume zone argument. */

	return it8951_sg_display_area(data, memaddr, mode, &zone);
}

static int do_pmic_cmd(struct it8951_data *data,
		       const char *arg_vcom, const char *arg_pwr)
{
	uint16_t vcom, *vcom_ptr = NULL;
	uint8_t pwr, *pwr_ptr = NULL;

	if (!arg_vcom && !arg_pwr) {
		fprintf(stderr, "Missing argument for pmic command\n");
		return EINVAL;
	}
	if (arg_vcom && sscanf(arg_vcom, "%hd", &vcom) == 1) {
		vcom_ptr = &vcom;
		optind++; /* Consume vcom argument. */
	}
	if (arg_pwr) {
		if (!strcmp(arg_pwr, "on")) {
			pwr = 1;
			pwr_ptr = &pwr;
			optind++; /* Consume power argument. */
		} else if (!strcmp(arg_pwr, "off")) {
			pwr = 0;
			pwr_ptr = &pwr;
			optind++; /* Consume power argument. */
		} else {
			fprintf(stderr,
				"Invalid argument %s for power command\n",
				arg_pwr);
			return EINVAL;
		}
	}
	return it8951_sg_pmic(data, vcom_ptr, pwr_ptr);

}

int main(int argc, char *argv[])
{
	int ret = 0;
	struct it8951_data *data;
	uint32_t mode = 2; /* FIXME: default waveform mode. */
	uint32_t memaddr = 0;
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
		case 'w': /* --waveform */
			mode = atoi(optarg);
			break;
		default:
			usage();
			return EINVAL;
		}
	}

	/* Device argument. */
	if (!argv[optind]) {
		fprintf(stderr, "Missing device name argument\n");
		return EINVAL;
	}
	ret = it8951_sg_open(&data, argv[optind++]);
	if (ret)
		return ret;

	if (!memaddr)
		memaddr = data->dev->memaddr;

	/* Commands arguments. */
	if (!argv[optind]) {
		fprintf(stderr, "Missing command arguments\n");
		return EINVAL;
	}

	do {
		char *cmd = argv[optind++];
		const char *next = argv[optind];
		const char *nextnext = NULL;

		if (!strcmp(cmd, "info")) {
			it8951_sg_info(data);
			ret = 0;
			continue;
		}
		if (!strcmp(cmd, "write")) {
			ret = do_write_mem_cmd(data, memaddr, false, next);
			continue;
		}
		if (!strcmp(cmd, "fwrite")) {
			ret = do_write_mem_cmd(data, memaddr, true, next);
			continue;
		}
		if (!strcmp(cmd, "read")) {
			ret = do_read_mem_cmd(data, memaddr, next);
			continue;
		}
		if (!strcmp(cmd, "load")) {
			if (next)
				nextnext = argv[optind + 1];
			ret = do_load_area_cmd(data, memaddr, next, nextnext);
			continue;
		}
		if (!strcmp(cmd, "display")) {
			ret = do_display_area_cmd(data, memaddr, mode, next);
			continue;
		}
		if (!strcmp(cmd, "clear")) {
			/* FIXME: waveform mode 0 seems to clear the screen. */
			ret = do_display_area_cmd(data, memaddr, 0, next);
			continue;
		}
		if (!strcmp(cmd, "vcom")) {
			ret = do_pmic_cmd(data, next, NULL);
			continue;
		}
		if (!strcmp(cmd, "power")) {
			ret = do_pmic_cmd(data, NULL, next);
			continue;
		}

		fprintf(stderr, "Unknown command %s\n", cmd);
		ret = EINVAL;
	} while (!ret && argv[optind]);

	it8951_sg_close(data);

	return ret;
}
