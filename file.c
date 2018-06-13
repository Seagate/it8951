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
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <errno.h>

#include "debug.h"

int read_buf_from_file(const char *fname, char **buf, size_t *size)
{
	FILE *f;
	struct stat sb;
	int ret;
	size_t read;

	debug("Reading %s\n", fname);

	f = fopen(fname, "r");
	if (!f) {
		ret = errno;
		err("Failed to fopen file %s: %s\n", fname, strerror(errno));
		return ret;
	}
	if (stat(fname, &sb) == -1) {
		ret = errno;
		err("Failed to stat file %s: %s\n", fname, strerror(errno));
		goto exit_close;
	}
	*buf = malloc(sb.st_size);
        if (!*buf) {
		ret = ENOMEM;
                err("Failed to malloc %ld bytes: %s\n",
		    sb.st_size, strerror(errno));
		goto exit_close;
        }

	*size = 0;
	do {
		read = fread(*buf + *size, 1, sb.st_size - *size, f);
		*size += read;
	} while (read);

	if (*size < sb.st_size) {
		ret = EIO;
		err("Only %ld bytes read from %s (%ld expected)\n",
		    read, fname, sb.st_size);
		goto exit_close;
	}
	ret = 0;
exit_close:
	fclose(f);
	return ret;
}

int write_buf_to_file(const char *fname, char *buf, size_t size)
{
	FILE *f;
	size_t written;
	int ret;

	info("Writing %ld bytes in %s\n", size, fname);

	f = fopen(fname, "w");
	if (!f) {
		ret = errno;
		err("Failed to fopen file %s: %s\n", fname, strerror(errno));
		return ret;
	}

	do {
		written = fwrite(buf, 1, size, f);
		size -= written;
	} while (written && size);

	if (size) {
		ret = EIO;
		err("Only %ld bytes written to %s (%ld expected)\n",
		    written, fname, written + size);
		goto exit_close;
	}

	ret = 0;
exit_close:
	fclose(f);
	return ret;
}
