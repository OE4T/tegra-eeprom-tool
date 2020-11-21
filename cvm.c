/*
 * cvm.c
 *
 * SoC-specific functions.
 *
 * Copyright (c) 2020 Matthew Madison
 */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "cvm.h"

static const struct cvm_i2c_address_s cvm_addr[TEGRA_SOCTYPE_COUNT] = {
	[TEGRA_SOCTYPE_186] = { 7, 0x50 },
	[TEGRA_SOCTYPE_194] = { 0, 0x50 },
	[TEGRA_SOCTYPE_210] = { 2, 0x50 },
};

/*
 * cvm_soctype
 */
tegra_soctype_t
cvm_soctype (void) {
	ssize_t typelen;
	int fd;
	char soctype[65];
	unsigned long chipid;

	fd = open("/sys/module/tegra_fuse/parameters/tegra_chip_id", O_RDONLY);
	if (fd < 0)
		return TEGRA_SOCTYPE_INVALID;
	typelen = read(fd, soctype, sizeof(soctype)-1);
	close(fd);
	if (typelen < 0)
		return TEGRA_SOCTYPE_INVALID;
	while (typelen > 0 && soctype[typelen-1] == '\n') typelen--;

	soctype[typelen] = '\0';

	chipid = strtoul(soctype, NULL, 10);
	switch (chipid) {
	case 0x18:
		return TEGRA_SOCTYPE_186;
	case 0x19:
		return TEGRA_SOCTYPE_194;
	case 0x21:
		return TEGRA_SOCTYPE_210;
	default:
		break;
	}
	return TEGRA_SOCTYPE_INVALID;

} /* cvm_soctype */

/*
 * cvm_soctype_name
 */
const char *
cvm_soctype_name (tegra_soctype_t soctype)
{
	switch (soctype) {
	case TEGRA_SOCTYPE_186:
		return "Tegra186";
	case TEGRA_SOCTYPE_194:
		return "Tegra194";
	case TEGRA_SOCTYPE_210:
		return "Tegra210";
	default:
		break;
	}
	return "INVALID";

} /* cvm_soctype_name */

/*
 * cvm_i2c_address
 */
const cvm_i2c_address_t *
cvm_i2c_address (void) {
	tegra_soctype_t s = cvm_soctype();

	if (s == TEGRA_SOCTYPE_INVALID)
		return NULL;

	return &cvm_addr[s];

} /* cvm_i2c_address*/
