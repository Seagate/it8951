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
#include <ctype.h>
#include <sys/stat.h>

#include "debug.h"
#include "image.h"

#define MAX_IMAGE_SIZE (2048*2048)

/*
 * Read the header contents of a PGM (Portable Grey[scale] Map) file.
 * An ASCII PGM image file follows the format:
 * P2
 * <X> <Y>
 * <levels>
 * <I1> <I2> ... <IMAX>
 * A binary PGM image file uses P5 instead of P2 and
 * the data values are represented in binary.
 * NOTE1: Comment lines start with '#'.
 * NOTE2: < > denote integer values (in decimal).
 */
static int read_pgm_header(FILE *f, struct image *img)
{
	int token = 0;
	char line[64];
	char magic[4];
	int x, y, maxcolor;

	while (fgets(line, sizeof(line), f) != NULL) {
		int i, found = 0;

		for (i = 0; line[i] != '\0'; i++) {
			if (isgraph(line[i]))
				break;
		}
		if (line[i] == '#')
			continue;

		if (token == 0)
			found = sscanf(line, "%02s %d %d %d",
					magic, &x, &y, &maxcolor);
		else if (token == 1)
			found = sscanf(line, "%d %d %d", &x, &y, &maxcolor);
		else if (token == 2)
			found = sscanf(line, "%d %d", &y, &maxcolor);
		else if (token == 3)
			found = sscanf(line, "%d", &maxcolor);

		if (found == EOF)
			return EINVAL;

		token += found;
		if (token == 4)
			break;
	}

	if (token != 4)
		return EINVAL;

	if (strcmp(magic, "P5"))
		return EINVAL;

	if (x <= 0 || y <= 0 || maxcolor <= 0)
		return EINVAL;

	img->width = x;
	img->height = y;
	img->maxcolor = maxcolor;
	img->type = pgm_bin;

	return 0;
}

static int read_pgm_image(FILE *f, struct image *img, size_t fsize)
{
	size_t read = 0, size = 0;

	do {
		read = fread(img->buf + size, 1, fsize, f);
		size += read;
	} while (read);

	if (size != img->width * img->height) {
		err("image: read %ld bytes, expected %d bytes (%dx%d)\n",
		    size, img->width * img->height, img->width, img->height);
		return EINVAL;
	}

	return 0;
}

static int write_pgm_image(FILE *f, struct image *img)
{
	size_t written;
	size_t towrite = img->width * img->height;

	/* PGM header. */
	fprintf(f, "P5\n%d %d\n255\n", img->width, img->height);

	do {
		written = fwrite(img->buf, 1, towrite, f);
		towrite -= written;
	} while (written && towrite);

	return towrite;
}

static struct image *load_image_from_file(const char *filename)
{
	FILE *f;
	struct stat sb;
	struct image *img;

	info("image: loading from file %s\n", filename);

	f = fopen(filename, "r");
	if (!f) {
		err("image: failed to fopen file %s: %s\n",
		    filename, strerror(errno));
		goto exit;
	}
	if (stat(filename, &sb) == -1) {
		err("image: failed to stat file %s: %s\n",
		    filename, strerror(errno));
		goto exit_close;
	}
	img = alloc_image(sb.st_size);
	if (!img)
		goto exit_close;
	if (read_pgm_header(f, img)) {
		err("image: failed to read PGM header in file %s\n", filename);
		goto exit_free;
	}

	info("image: found PGM header - x=%d y=%d maxcolor=%d\n",
	     img->width, img->height, img->maxcolor);

	if (read_pgm_image(f, img, sb.st_size)) {
		err("image: failed to read PGM image in file %s\n", filename);
		goto exit_free;
	}
	fclose(f);

	return img;

exit_free:
	free(img);
exit_close:
	fclose(f);
exit:
	return NULL;
}

static struct image *
build_monochrome_image(unsigned int width, unsigned int height,
		       unsigned char color)
{
	struct image *img = NULL;

	info("image: build monochrome image %dx%d (color=%d)\n",
	     width, height, color);

	if (width * height > MAX_IMAGE_SIZE) {
		err("image: size too large: %d bytes\n", width * height);
		return NULL;
	}

	img = (struct image *) malloc(sizeof(*img) + width * height);
	if (!img) {
		err("image: failed to malloc %ld bytes: %s\n",
		    sizeof(*img) + width * height, strerror(errno));
		return NULL;
	}

	memset(img->buf, color, width * height);
	img->width = width;
	img->height = height;
	img->maxcolor = color;
	img->type = pgm_bin;

	return img;
}

struct image *alloc_image(size_t size)
{
	struct image *img = NULL;

	if (size > MAX_IMAGE_SIZE) {
		err("image: size too large: %ld bytes\n", size);
		return NULL;
	}
	img = (struct image *) malloc(sizeof(*img) + size);
	if (!img)
		err("image: failed to malloc %ld bytes: %s\n",
		    sizeof(*img) + size, strerror(errno));

	return img;
}

struct image *load_image(const char *name)
{
	int match;
	unsigned int width, height;
	unsigned char color;

	/*
	 * The name can be either a magic string used to build a monochrome
	 * image. Format: ${width}x${height}x${color[0-255]}.
	 */
	match = sscanf(name, "%dx%dx%hhd", &width, &height, &color);
	if (match == 3)
		return build_monochrome_image(width, height, color);

	/* Or a file name. */
	return load_image_from_file(name);
}

/*
 * This function saves an image buffer into a file (PGM format) under /tmp.
 */
int save_image_to_file(struct image *img)
{
	FILE *f;
	char filename[64];
	int err;

	snprintf(filename, sizeof(filename), "/tmp/it8951-%dx%d.pgm",
		 img->width, img->height);

	info("image: saving to %s\n", filename);

	f = fopen(filename, "w");
	if (!f) {
		err("image: failed to fopen file %s: %s\n",
		    filename, strerror(errno));
		return errno;
	}
	err = write_pgm_image(f, img);
	if (err)
		err("image: failed to write PGM image\n");

	fclose(f);

	return err;
}
