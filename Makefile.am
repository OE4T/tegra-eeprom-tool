# Tegra EEPROM tools makefile
#
# Copyright (c) 2020 Matthew Madison
#

AUTOMAKE_OPTIONS = subdir-objects foreign
ACLOCAL_AMFLAGS = -I m4
SUBDIRS =
DIST_SUBDIRS =

prefix = /usr
exec_prefix = /usr
bindir = $(exec_prefix)/bin

EXTRA_DIST = README.md LICENSE

bin_PROGRAMS = tegra-eeprom-tool tegra-boardspec
tegra_eeprom_tool_SOURCES = tegra-eeprom-tool.c eeprom.c cvm.c eeprom.h cvm.h
tegra_eeprom_tool_CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE
tegra_eeprom_tool_CFLAGS = $(LIBEDIT_CFLAGS)
tegra_eeprom_tool_LDADD = $(LIBEDIT_LIBS)

tegra_boardspec_SOURCES = tegra-boardspec.c eeprom.c cvm.c eeprom.h cvm.h
