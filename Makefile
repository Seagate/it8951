#
# This Makefile is part of the it8951 collection of tools.
#
# Copyright (C) 2018-2020 Seagate Technology LLC
#
# it8951 is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# it8951 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with it8951.  If not, see <http://www.gnu.org/licenses/>.
#

# Build directory (can be overwritten)
O ?= .

# Files.
SRCS = $(wildcard *.c)
OBJS = $(SRCS:%.c=$O/%.o)

BINS = it8951_cmd it8951_flash it8951_fw
BUILD_BINS = $(BINS:%=$O/%)

# Build options.
CC ?= gcc
CFLAGS ?= -Wall -O3
CPPFLAGS ?= -DHAVE_GETOPT_LONG

# Install options.
FW_INSTALL_DIR ?= /lib/firmware/it8951

all: $(BUILD_BINS)

$O/%.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

$O/it8951_cmd: $O/cmd_main.o $O/file.o $O/image.o $O/sg.o $O/debug.o
	$(CC) $(LDFLAGS) $^ -o $@

$O/it8951_flash: $O/common.o $O/file.o $O/flash_main.o $O/image.o $O/sf.o $O/sg.o $O/debug.o
	$(CC) $(LDFLAGS) $^ -o $@

$O/it8951_fw: $O/common.o $O/file.o $O/fw.o $O/fw_main.o $O/image.o $O/sf.o $O/sg.o $O/debug.o
	$(CC) $(LDFLAGS) $^ -o $@

install: $(BUILD_BINS)
	@ for f in $(^F); do \
		install -vD -m0755 $O/$$f $(DESTDIR)/usr/sbin/$$f; \
	done
	@ install -d $(DESTDIR)$(FW_INSTALL_DIR)
	@ install -vD fw/* $(DESTDIR)$(FW_INSTALL_DIR)

uninstall:
	@ rm -vrf $(DESTDIR)$(FW_INSTALL_DIR)
	@ for f in $(BINS); do \
		rm -vf $(DESTDIR)/usr/sbin/$$f; \
	done

clean:
	@ rm -vf $(OBJS) $(BUILD_BINS)

.PHONY: all install uninstall clean
