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

#ifndef IMAGE_H
#define IMAGE_H

enum image_type {
	pgm_bin = 0,
};

struct image {
	int width;
	int height;
	int maxcolor;
	enum image_type type;
	char buf[];
};

struct image *alloc_image(size_t size);
struct image *load_image(const char *filename);
int save_image_to_file(struct image *img);
#endif
