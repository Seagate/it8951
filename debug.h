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

#ifndef DEBUG_H
#define DEBUG_H

enum log_level {
	ERR = 0,
	INFO,
	DEBUG,
};

void print_log(enum log_level level, const char *format, ...);

#define err(format, arg...) print_log(ERR, "[ERR] " format, ##arg)
#define info(format, arg...) print_log(INFO, "[INFO] " format, ##arg)
#define debug(format, arg...) print_log(DEBUG, "[DEBUG] " format, ##arg)

#endif
