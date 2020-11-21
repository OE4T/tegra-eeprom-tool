# Tegra EEPROM tools makefile
#
# Copyright (c) 2020 Matthew Madison
#

AUTOMAKE_OPTIONS = subdir-objects foreign
ACLOCAL_AMFLAGS = -I m4
SUBDIRS =
DIST_SUBDIRS =

eeincdir = $(includedir)/tegra-eeprom
pkgconfigdir = $(libdir)/pkgconfig

EXTRA_DIST = README.md LICENSE

lib_LTLIBRARIES = libtegra-eeprom.la
libtegra_eeprom_la_SOURCES = eeprom.c eeprom.h cvm.c cvm.h boardspec.c boardspec.h
eeinc_HEADERS = eeprom.h cvm.h boardspec.h

bin_PROGRAMS = tegra-eeprom-tool tegra-boardspec
tegra_eeprom_tool_SOURCES = tegra-eeprom-tool.c
tegra_eeprom_tool_CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE
tegra_eeprom_tool_CFLAGS = $(LIBEDIT_CFLAGS)
tegra_eeprom_tool_LDADD = libtegra-eeprom.la $(LIBEDIT_LIBS)

tegra_boardspec_SOURCES = tegra-boardspec.c
tegra_boardspec_LDADD = libtegra-eeprom.la

pkgconfig_DATA = tegra-eeprom.pc
