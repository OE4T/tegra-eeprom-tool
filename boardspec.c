/*
 * boardspec
 *
 * API for extracting board information
 * from the CVM EEPROM.
 *
 * Copyright (c) 2020 Matthew Madison
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include "cvm.h"
#include "eeprom.h"

/*
 * get_prod_mode
 *
 */
static int
get_prod_mode (unsigned long *mode) {
	ssize_t modelen;
	int fd;
	char prod_mode[65];

	if (mode == NULL) {
		errno = EINVAL;
		return -1;
	}
	fd = open("/sys/module/tegra_fuse/parameters/tegra_prod_mode", O_RDONLY);
	if (fd < 0)
		return fd;
	modelen = read(fd, prod_mode, sizeof(prod_mode)-1);
	close(fd);
	if (modelen < 0)
		return modelen;
	while (modelen > 0 && prod_mode[modelen-1] == '\n') modelen--;

	prod_mode[modelen] = '\0';

	*mode = strtoul(prod_mode, NULL, 10);
	return 0;

} /* get_prod_mode */

/*
 * tegra_boardspec
 *
 * Formats the boardspec for the current system
 * into a caller-provided buffer.
 *
 * Returns: length of string, or negative integer on error.
 */
int
tegra_boardspec (char *buf, unsigned int bufsiz)
{
	eeprom_context_t ectx = NULL;
	module_eeprom_t eeprom;
	tegra_soctype_t soctype;
	const cvm_i2c_address_t *addr;
	ssize_t len;
	unsigned long prod_mode;
	char eeprompath[PATH_MAX];
	char boardrev[4];

	if (get_prod_mode(&prod_mode) < 0)
		return -1;

	soctype = cvm_soctype();
	addr = cvm_i2c_address();
	if (soctype == TEGRA_SOCTYPE_INVALID || addr == NULL) {
		errno = ENODEV;
		return -1;
	}

	len = snprintf(eeprompath, sizeof(eeprompath)-1,
		       "/sys/bus/i2c/devices/%d-%04x/eeprom",
		       addr->busnum, addr->addr);
	if (len > 0) {
		eeprompath[len] = '\0';
		if (access(eeprompath, F_OK) == 0)
			ectx = eeprom_open(eeprompath, module_type_cvm);
	}
	if (ectx == NULL)
		ectx = eeprom_open_i2c(addr->busnum, addr->addr, module_type_cvm);
	if (ectx == NULL)
		return -1;
	if (eeprom_read(ectx, &eeprom) != 0) {
		eeprom_close(ectx);
		return -1;
	}
	eeprom_close(ectx);
	if (eeprom.partnumber_type != partnum_type_nvidia) {
		errno = ENOMSG;
		return -1;
	}
	/*
	 * Part number is 699-8bbbb-ssss-fff RRR:
	 *   bbbb = boardid
	 *   ssss = boardsku
	 *   fff  = fab
	 *   RRR  = boardrev
	 * Have seen some EEPROMs with non-existent
	 * or shorter boardrevs, so make sure the blank
	 * is present and at least one char is printable.
	 *
	 */
	memset(boardrev, 0, sizeof(boardrev));
	if (eeprom.partnumber[18] == ' ' && isprint(eeprom.partnumber[19]))
		memcpy(boardrev, &eeprom.partnumber[19], 3);
	/*
	 * XXX punt on the chip revision for now,
	 * should be 0 for non-T194 parts, 2 for T194.
	 * Could read the chip rev from sysfs, but
	 * the value is a kernel-defined enum that
	 * could change.
	 */
	return snprintf(buf, bufsiz,
			"%-4.4s-%-3.3s-%-4.4s-%s-%lu-%u",
			&eeprom.partnumber[5],
			&eeprom.partnumber[15],
			&eeprom.partnumber[10],
			boardrev,
			prod_mode,
			(soctype == TEGRA_SOCTYPE_194 ? 2 : 0));

} /* tegra_boardspec */
