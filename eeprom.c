// Copyright (c) 2019-2022, Matthew Madison
//
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <errno.h>
#include "eeprom.h"
#include "cvm.h"

#define LAYOUT_VERSION_V1	1U
#define LAYOUT_VERSION_V2	2U
#define LAYOUT_VERSION_NON_T234	LAYOUT_VERSION_V1
#define LAYOUT_VERSION_T234	LAYOUT_VERSION_V2
#define LAYOUT_VERSION_T264	LAYOUT_VERSION_V2
static const char cfgblk_sig[4] = "NVCB";
static const char cfgblk_none[4] = "FFFF";
#define CFGBLK_LENGTH	28
static const char macfmt_tag[2] = "M1";
static const uint8_t macaddr_placeholder[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#define MACFMT_VERSION  0
struct module_eeprom_v1_raw {
	uint8_t  major_version;
	uint8_t  minor_version;
	uint16_t length;
	uint8_t  reserved_1__[15];
	uint8_t  ether_mac_count_v2;
	char     partnumber[22];
	uint8_t  padding[8]; // either 0 or FF
	uint8_t  factory_default_wifi_mac[6]; // little-endian
	uint8_t  factory_default_bt_mac[6];
	uint8_t  factory_default_wifi_alt_mac[6];
	uint8_t  factory_default_ether_mac[6];
	char     asset_id[15]; // string padded with 0 or FF
	uint8_t  reserved_2__[61];
	// Begin vendor block
	char     cfgblk_sig[4];
	uint16_t cfgblk_len;
	char     macfmt_tag[2];
	uint16_t macfmt_version;
	uint8_t  vendor_wifi_mac[6];
	uint8_t  vendor_bt_mac[6];
	uint8_t  vendor_ether_mac[6];
	uint8_t  vendor_ether_mac_count_v2;
	uint8_t  reserved_3__[21];
	// End vendor block, begin system-level information
	char     system_partnumber_v2[21];
	char     system_serialnumber_v2[15];
	uint8_t  reserved_4__[19];
	uint8_t  crc8;
} __attribute__((packed));

struct eeprom_context_s {
	int fd;
	int readonly;
	tegra_soctype_t soctype;
	eeprom_module_type_t mtype;
	struct module_eeprom_v1_raw eeprom_data;
};

typedef ssize_t (*eeprom_readfunc_t)(int fd, void *buf, size_t bufsize);

/*
 * This table was generated using the Python code in the 'Jetson TX1/TX2 Module EEPROM Layout'
 * document downloaded from NVIDIA's web site.
 */
static const uint8_t crc_table[256] =
{
	0x00, 0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd, 0x83, 0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41,
	0x9d, 0xc3, 0x21, 0x7f, 0xfc, 0xa2, 0x40, 0x1e, 0x5f, 0x01, 0xe3, 0xbd, 0x3e, 0x60, 0x82, 0xdc,
	0x23, 0x7d, 0x9f, 0xc1, 0x42, 0x1c, 0xfe, 0xa0, 0xe1, 0xbf, 0x5d, 0x03, 0x80, 0xde, 0x3c, 0x62,
	0xbe, 0xe0, 0x02, 0x5c, 0xdf, 0x81, 0x63, 0x3d, 0x7c, 0x22, 0xc0, 0x9e, 0x1d, 0x43, 0xa1, 0xff,
	0x46, 0x18, 0xfa, 0xa4, 0x27, 0x79, 0x9b, 0xc5, 0x84, 0xda, 0x38, 0x66, 0xe5, 0xbb, 0x59, 0x07,
	0xdb, 0x85, 0x67, 0x39, 0xba, 0xe4, 0x06, 0x58, 0x19, 0x47, 0xa5, 0xfb, 0x78, 0x26, 0xc4, 0x9a,
	0x65, 0x3b, 0xd9, 0x87, 0x04, 0x5a, 0xb8, 0xe6, 0xa7, 0xf9, 0x1b, 0x45, 0xc6, 0x98, 0x7a, 0x24,
	0xf8, 0xa6, 0x44, 0x1a, 0x99, 0xc7, 0x25, 0x7b, 0x3a, 0x64, 0x86, 0xd8, 0x5b, 0x05, 0xe7, 0xb9,
	0x8c, 0xd2, 0x30, 0x6e, 0xed, 0xb3, 0x51, 0x0f, 0x4e, 0x10, 0xf2, 0xac, 0x2f, 0x71, 0x93, 0xcd,
	0x11, 0x4f, 0xad, 0xf3, 0x70, 0x2e, 0xcc, 0x92, 0xd3, 0x8d, 0x6f, 0x31, 0xb2, 0xec, 0x0e, 0x50,
	0xaf, 0xf1, 0x13, 0x4d, 0xce, 0x90, 0x72, 0x2c, 0x6d, 0x33, 0xd1, 0x8f, 0x0c, 0x52, 0xb0, 0xee,
	0x32, 0x6c, 0x8e, 0xd0, 0x53, 0x0d, 0xef, 0xb1, 0xf0, 0xae, 0x4c, 0x12, 0x91, 0xcf, 0x2d, 0x73,
	0xca, 0x94, 0x76, 0x28, 0xab, 0xf5, 0x17, 0x49, 0x08, 0x56, 0xb4, 0xea, 0x69, 0x37, 0xd5, 0x8b,
	0x57, 0x09, 0xeb, 0xb5, 0x36, 0x68, 0x8a, 0xd4, 0x95, 0xcb, 0x29, 0x77, 0xf4, 0xaa, 0x48, 0x16,
	0xe9, 0xb7, 0x55, 0x0b, 0x88, 0xd6, 0x34, 0x6a, 0x2b, 0x75, 0x97, 0xc9, 0x4a, 0x14, 0xf6, 0xa8,
	0x74, 0x2a, 0xc8, 0x96, 0x15, 0x4b, 0xa9, 0xf7, 0xb6, 0xe8, 0x0a, 0x54, 0xd7, 0x89, 0x6b, 0x35
};

static uint8_t
calc_crc8 (uint8_t *buf, size_t buflen)
{
	uint8_t crc = 0;
	while (buflen-- > 0)
		crc = crc_table[(crc ^ *buf++) & 0xff];
	return crc;

} /* calc_crc8 */

/*
 * eeprom_data_valid
 *
 * Verify CRC and check that the version and tag fields
 * are ones we recognize.
 */
int
eeprom_data_valid (eeprom_context_t ctx)
{
	struct module_eeprom_v1_raw *data = &ctx->eeprom_data;
	if (data->crc8 != calc_crc8((uint8_t *) data, 255))
		return 0;
	if (ctx->soctype == TEGRA_SOCTYPE_234) {
		if (data->major_version != LAYOUT_VERSION_T234)
			return 0;
	} else if (ctx->soctype == TEGRA_SOCTYPE_264) {
		if (data->major_version != LAYOUT_VERSION_T264)
			return 0;
	} else if (data->major_version != LAYOUT_VERSION_NON_T234)
		return 0;
	if (ctx->mtype == module_type_cvm) {
		if (memcmp(data->cfgblk_sig, cfgblk_sig, sizeof(cfgblk_sig)))
			return 0;
		if (memcmp(data->macfmt_tag, macfmt_tag, sizeof(macfmt_tag)))
			return 0;
		if (le16toh(data->macfmt_version) != MACFMT_VERSION)
			return 0;
	}
	return 1;

} /* eeprom_data_valid */

/*
 * strings in the EEPROM fields may be padded with either
 * nulls or 0xff
 */
static void
extract_string (char *dst, const char *src, size_t maxlen)
{
	size_t len = maxlen;
	uint8_t *s = (uint8_t *) src;

	if (s[len-1] == 0xff)
		while (len > 0 && s[len-1] == 0xff)
			len -= 1;
	else if (s[len-1] == '\0')
		while (len > 0 && s[len-1] == '\0')
			len -= 1;
	if (len > 0)
		memcpy(dst, src, len);
	if (len < maxlen)
		memset(dst+len, 0, maxlen-len);

} /* extract_string*/

/*
 * extract_macaddr
 *
 * Byte-reversed copy of 6 bytes.
 */
static void
extract_macaddr (uint8_t *dst, const uint8_t *src)
{
	int i;
	for (i = 0; i < 6; i++)
		dst[5-i] = src[i];

} /* extract_macaddr */

/*
 * normal_read
 *
 * Used when we're talking through an EEPROM driver.
 */
ssize_t
normal_read (int fd, void *buf, size_t bufsize)
{
	uint8_t *bp;
	ssize_t len;
	size_t offset;

	for (bp = buf, offset = 0; offset < bufsize; offset += len, bp += len) {
		len = read(fd, bp, bufsize-offset);
		if (len < 0)
			return len;
	}

	return bufsize;

} /* normal_read */

/*
 * smbus_read
 *
 * Used when we're talking through the I2C driver.
 */
ssize_t
smbus_read (int fd, void *buf, size_t bufsize)
{
	uint8_t *bp = buf;
	size_t offset;
	int err;

	union i2c_smbus_data data;
	struct i2c_smbus_ioctl_data args = {
		.read_write = I2C_SMBUS_READ,
		.size = I2C_SMBUS_BYTE_DATA,
		.data = &data,
	};

	for (offset = 0; offset < bufsize; offset += 1) {
		args.command = offset;
		err = ioctl(fd, I2C_SMBUS, &args);
		if (err < 0)
			return err;
		*bp++ = data.byte & 0xFF;
	}
	return (ssize_t) offset;

} /* smbus_read */

/*
 * open_common
 */
static eeprom_context_t
open_common (int fd, eeprom_module_type_t mtype, int readonly, eeprom_readfunc_t readfunc)
{
	eeprom_context_t ctx;
	tegra_soctype_t soctype = cvm_soctype();

	if (soctype == TEGRA_SOCTYPE_INVALID) {
		errno = EINVAL;
		return NULL;
	}
	ctx = calloc(1, sizeof(struct eeprom_context_s));
	if (ctx == NULL) {
		close(fd);
		return ctx;
	}
	ctx->soctype = soctype;
	ctx->mtype = mtype;
	ctx->fd = fd;
	ctx->readonly = readonly;
	if (readfunc(fd, &ctx->eeprom_data, sizeof(ctx->eeprom_data)) < 0) {
		close(fd);
		free(ctx);
		return NULL;
	}
	return ctx;
}

/*
 * eeprom_open_i2c
 *
 * for module EEPROMs that aren't controlled by a driver
 */
eeprom_context_t
eeprom_open_i2c (unsigned int bus, unsigned int addr, eeprom_module_type_t mtype)
{
	char devname[32];
	ssize_t len;
	int fd;


	len = snprintf(devname, sizeof(devname)-1, "/dev/i2c-%u", bus);
	if (len < 0)
		return NULL;
	fd = open(devname, O_RDWR);
	if (fd < 0)
		return NULL;
	if (ioctl(fd, I2C_SLAVE_FORCE, addr) < 0) {
		close(fd);
		return NULL;
	}

	return open_common(fd, mtype, 1, smbus_read);

} /* eeprom_open_i2c */

/*
 * eeprom_open
 *
 * for module EEPROMs that are controlled by
 * an eeprom driver, or files that have EEPROM contents
 */
eeprom_context_t
eeprom_open (const char *pathname, eeprom_module_type_t mtype)
{
	int fd;
	int readonly = 0;

	fd = open(pathname, O_RDWR);
	if (fd < 0) {
		fd = open(pathname, O_RDONLY);
		readonly = 1;
	}
	if (fd < 0)
		return NULL;
	return open_common(fd, mtype, readonly, normal_read);

} /* eeprom_open */

/*
 * eeprom_close
 *
 * Clean up context.
 */
void
eeprom_close (eeprom_context_t ctx)
{
	close(ctx->fd);
	free(ctx);

} /* eeprom_close */

/*
 * eeprom_readonly
 *
 * returns 1 if open read-only, 0 otherwise.
 */
int
eeprom_readonly (eeprom_context_t ctx)
{
	return ctx->readonly;

} /* eeprom_readonly */

/*
 * eeprom_read
 *
 * Validate the EEPROM data obtained from the device
 * and extract the important data from it, translating
 * from the raw form into something usable: mainly
 * converting MAC addresses from little-endian format
 * into the more-typical big-endian format.
 *
 */
int
eeprom_read (eeprom_context_t ctx, module_eeprom_t *data)
{
	struct module_eeprom_v1_raw *rawdata = &ctx->eeprom_data;

	memset(data, 0, sizeof(module_eeprom_t));

	if (!eeprom_data_valid(ctx)) {
		errno = EFAULT;
		return -1;
	}

	if ((uint8_t) rawdata->partnumber[0] == 0xcc) {
		data->partnumber_type = partnum_type_customer;
		extract_string(data->partnumber, rawdata->partnumber+1, sizeof(rawdata->partnumber)-1);
	} else {
		data->partnumber_type = partnum_type_nvidia;
		extract_string(data->partnumber, rawdata->partnumber, sizeof(rawdata->partnumber));
	}
	extract_string(data->asset_id, rawdata->asset_id, sizeof(rawdata->asset_id));
	extract_macaddr(data->factory_default_wifi_mac, rawdata->factory_default_wifi_mac);
	extract_macaddr(data->factory_default_bt_mac, rawdata->factory_default_bt_mac);
	extract_macaddr(data->factory_default_wifi_alt_mac, rawdata->factory_default_wifi_alt_mac);
	extract_macaddr(data->factory_default_ether_mac, rawdata->factory_default_ether_mac);
	extract_macaddr(data->vendor_wifi_mac, rawdata->vendor_wifi_mac);
	extract_macaddr(data->vendor_bt_mac, rawdata->vendor_bt_mac);
	extract_macaddr(data->vendor_ether_mac, rawdata->vendor_ether_mac);
	data->major_version = rawdata->major_version;
	data->minor_version = rawdata->minor_version;
	if (rawdata->major_version >= LAYOUT_VERSION_V2) {
		data->factory_default_ether_mac_count = rawdata->ether_mac_count_v2;
		data->vendor_ether_mac_count = rawdata->vendor_ether_mac_count_v2;
		extract_string(data->system_partnumber, rawdata->system_partnumber_v2, sizeof(rawdata->system_partnumber_v2));
		extract_string(data->system_serialnumber, rawdata->system_serialnumber_v2, sizeof(rawdata->system_serialnumber_v2));
	}
	return 0;

} /* eeprom_read */

/*
 * eeprom_write
 *
 * Writes module EEPROM data to the device.
 *
 * WARNING:
 *    Performs a complete overwrite, so to prevent
 *    losing data, you MUST call eeprom_read() to
 *    populate the module_eeprom structure, make any
 *    updates you need to, then call this function.
 *
 */
int
eeprom_write (eeprom_context_t ctx, module_eeprom_t *data)
{
	struct module_eeprom_v1_raw *rawdata = &ctx->eeprom_data;
	uint8_t *bp;
	size_t remain;
	ssize_t n;

	if (ctx->readonly) {
		errno = EROFS;
		return -1;
	}

	if ((ctx->soctype == TEGRA_SOCTYPE_234 && data->major_version != LAYOUT_VERSION_T234) ||
	    (ctx->soctype == TEGRA_SOCTYPE_264 && data->major_version != LAYOUT_VERSION_T264) ||
	    (ctx->soctype != TEGRA_SOCTYPE_234 && ctx->soctype != TEGRA_SOCTYPE_264 && data->major_version != LAYOUT_VERSION_NON_T234)) {
		errno = EINVAL;
		return -1;
	};

	if (!eeprom_data_valid(ctx)) {
		memset(rawdata, 0, sizeof(*rawdata));
		if (ctx->soctype == TEGRA_SOCTYPE_234)
			rawdata->major_version = LAYOUT_VERSION_T234;
		else if (ctx->soctype == TEGRA_SOCTYPE_264)
			rawdata->major_version = LAYOUT_VERSION_T264;
		else
			rawdata->major_version = LAYOUT_VERSION_NON_T234;
		if (ctx->soctype == TEGRA_SOCTYPE_234)
			rawdata->major_version = LAYOUT_VERSION_T234;
		else if (ctx->soctype == TEGRA_SOCTYPE_264)
			rawdata->major_version = LAYOUT_VERSION_T264;
		else
			rawdata->major_version = LAYOUT_VERSION_NON_T234;
		if (ctx->mtype == module_type_cvm) {
			memcpy(rawdata->cfgblk_sig, cfgblk_sig, sizeof(rawdata->cfgblk_sig));
			rawdata->cfgblk_len = CFGBLK_LENGTH;
			memcpy(rawdata->macfmt_tag, macfmt_tag, sizeof(rawdata->macfmt_tag));
			rawdata->macfmt_version = htole16(MACFMT_VERSION);
		}
	}

	if (data->partnumber_type == partnum_type_nvidia)
		memcpy(rawdata->partnumber, data->partnumber, sizeof(rawdata->partnumber));
	else {
		rawdata->partnumber[0] = (char) 0xcc;
		memcpy(&rawdata->partnumber[1], data->partnumber, sizeof(rawdata->partnumber)-1);
	}
	memcpy(rawdata->asset_id, data->asset_id, sizeof(rawdata->asset_id));
	if (ctx->mtype == module_type_cvm) {
		extract_macaddr(rawdata->factory_default_wifi_mac, data->factory_default_wifi_mac);
		extract_macaddr(rawdata->factory_default_bt_mac, data->factory_default_bt_mac);
		extract_macaddr(rawdata->factory_default_wifi_alt_mac, data->factory_default_wifi_alt_mac);
		extract_macaddr(rawdata->factory_default_ether_mac, data->factory_default_ether_mac);
		extract_macaddr(rawdata->vendor_wifi_mac, data->vendor_wifi_mac);
		extract_macaddr(rawdata->vendor_bt_mac, data->vendor_bt_mac);
		extract_macaddr(rawdata->vendor_ether_mac, data->vendor_ether_mac);
	} else {
		memcpy(rawdata->cfgblk_sig, cfgblk_none, sizeof(rawdata->cfgblk_sig));
		memcpy(rawdata->macfmt_tag, cfgblk_none, sizeof(rawdata->macfmt_tag));
		memcpy(rawdata->factory_default_wifi_mac, macaddr_placeholder, 6);
		memcpy(rawdata->factory_default_bt_mac, macaddr_placeholder, 6);
		memcpy(rawdata->factory_default_wifi_alt_mac, macaddr_placeholder, 6);
		memcpy(rawdata->factory_default_ether_mac, macaddr_placeholder, 6);
		memcpy(rawdata->vendor_wifi_mac, macaddr_placeholder, 6);
		memcpy(rawdata->vendor_bt_mac, macaddr_placeholder, 6);
		memcpy(rawdata->vendor_ether_mac, macaddr_placeholder, 6);
	}
	if (data->major_version >= LAYOUT_VERSION_V2) {
		rawdata->ether_mac_count_v2 = data->factory_default_ether_mac_count;
		rawdata->vendor_ether_mac_count_v2 = data->vendor_ether_mac_count;
		// Per the L4T documentation, is field applies only to
		// carrier boards sold as part of NVIDIA development kits
		if (ctx->mtype == module_type_cvb)
			memcpy(rawdata->system_partnumber_v2, data->system_partnumber,
			       sizeof(rawdata->system_partnumber_v2));
		memcpy(rawdata->system_serialnumber_v2, data->system_serialnumber,
		       sizeof(rawdata->system_serialnumber_v2));
	}
	rawdata->length = htole16(sizeof(*rawdata) - 1);
	rawdata->crc8 = calc_crc8((uint8_t *) rawdata, 255);

	if (lseek(ctx->fd, 0, SEEK_SET) < 0)
		return -1;
	bp = (uint8_t *) rawdata;
	for (remain = sizeof(*rawdata); remain > 0; remain -= n, bp += n) {
		n = write(ctx->fd, bp, remain);
		if (n < 0)
			return -1;
	}
	return 0;

} /* eeprom_write */
