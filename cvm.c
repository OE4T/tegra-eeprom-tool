// Copyright (c) 2020 Matthew Madison
//
// SPDX-License-Identifier: MIT

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "cvm.h"

static const struct cvm_i2c_address_s cvm_addr[TEGRA_SOCTYPE_COUNT] = {
	[TEGRA_SOCTYPE_186] = { 7, 0x50 },
	[TEGRA_SOCTYPE_194] = { 0, 0x50 },
	[TEGRA_SOCTYPE_210] = { 2, 0x50 },
	[TEGRA_SOCTYPE_234] = { 0, 0x50 },
	[TEGRA_SOCTYPE_264] = { 1, 0x50 },
};

static const struct {
    tegra_soctype_t soctype;
    const char *compat;
} compat_info[] = {
	{ TEGRA_SOCTYPE_186, "nvidia,tegra186" },
	{ TEGRA_SOCTYPE_194, "nvidia,tegra194" },
	{ TEGRA_SOCTYPE_210, "nvidia,tegra210" },
	{ TEGRA_SOCTYPE_234, "nvidia,tegra234" },
	{ TEGRA_SOCTYPE_264, "nvidia,tegra264" },
};

/*
 * soctype_from_compat_strings
 */
static tegra_soctype_t
soctype_from_compat_strings (void)
{
	int fd;
	char buf[1024], *anchor, *cp;
	ssize_t n;
	size_t len;
	unsigned int i;

	fd = open("/proc/device-tree/compatible", O_RDONLY);
	if (fd < 0)
		return TEGRA_SOCTYPE_INVALID;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if (n <= 0)
		return TEGRA_SOCTYPE_INVALID;
	buf[n] = '\0';
	/*
	 * The compatible property is a sequence of null-terminated strings
	 */
	for (anchor = buf; anchor < buf + n && *anchor != '\0'; anchor = cp + 1) {
		cp = strchr(anchor, '\0');
		if (cp == NULL)
			break;
		len = cp - anchor;
		for (i = 0; i < sizeof(compat_info)/sizeof(compat_info[0]); i++) {
			if (len == strlen(compat_info[i].compat) &&
			    memcmp(compat_info[i].compat, anchor, len) == 0)
				return compat_info[i].soctype;
		}
	}
	return TEGRA_SOCTYPE_INVALID;

} /* soctype_from_compat_strings */

/*
 * cvm_soctype
 */
tegra_soctype_t
cvm_soctype (void)
{
	ssize_t typelen;
	int fd;
	char soctype[65];
	unsigned long chipid;

	fd = open("/sys/module/tegra_fuse/parameters/tegra_chip_id", O_RDONLY);
	if (fd < 0)
		return soctype_from_compat_strings();
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
	case 0x26:
		return TEGRA_SOCTYPE_264;
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
	case TEGRA_SOCTYPE_234:
		return "Tegra234";
	case TEGRA_SOCTYPE_264:
		return "Tegra264";
	default:
		break;
	}
	return "INVALID";

} /* cvm_soctype_name */

/*
 * cvm_i2c_address_for_soctype
 */
const cvm_i2c_address_t *
cvm_i2c_address_for_soctype (tegra_soctype_t s) {

	if ((int) s < 0 || s >= TEGRA_SOCTYPE_COUNT)
		return NULL;

	return &cvm_addr[s];

} /* cvm_i2c_address_for_soctype */

/*
 * cvm_i2c_address
 */
const cvm_i2c_address_t *
cvm_i2c_address (void) {
	return cvm_i2c_address_for_soctype(cvm_soctype());
} /* cvm_i2c_address */
