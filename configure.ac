dnl
dnl configure.ac - autoconf script for tegra-eeprom-tool
dnl
dnl Copyright (c) 2019-2020, Matthew Madison
dnl

AC_INIT([tegra-eeprom-tool], [1.1.1])
AC_DEFINE([EEPROMTOOL_VERSION_MAJOR], [1], [Major version])
AC_DEFINE([EEPROMTOOL_VERSION_MINOR], [1], [Minor version])
AC_DEFINE([EEPROMTOOL_VERSION_MAINT], [1], [Maintenance level])
AM_INIT_AUTOMAKE([subdir-objects foreign])
AM_SILENT_RULES([yes])
AC_COPYRIGHT([
Copyright (c) 2019-2020, Matthew Madison
])
AC_CONFIG_MACRO_DIR([m4])

AC_PREREQ([2.69])
LT_PREREQ([2.4])
LT_INIT([])

AC_CANONICAL_HOST
AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_CC_C_O

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([LIBEDIT], [libedit])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
