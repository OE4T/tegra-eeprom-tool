/*
 * tegra-boardspec
 *
 * Extracts board information from the CVM EEPROM.
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
#include "eeprom.h"

typedef enum {
	TEGRA_SOCTYPE_186,
	TEGRA_SOCTYPE_194,
	TEGRA_SOCTYPE_210,
	TEGRA_SOCTYPE_COUNT__
} tegra_soctype_t;
#define TEGRA_SOCTYPE_COUNT ((int) TEGRA_SOCTYPE_COUNT__)

static const struct cvm_i2c_address_s {
	int busnum;
	unsigned int addr;
} cvm_addr[TEGRA_SOCTYPE_COUNT] = {
	[TEGRA_SOCTYPE_186] = { 7, 0x50 },
	[TEGRA_SOCTYPE_194] = { 0, 0x50 },
	[TEGRA_SOCTYPE_210] = { 2, 0x50 },
};
	

static struct option options[] = {
	{ "help",		no_argument,		0, 'h' },
	{ 0,			0,			0, 0   }
};
static const char *shortopts = ":h";

static char *optarghelp[] = {
	"--help               ",
};

static char *opthelp[] = {
	"display this help text",
};

static char *progname;

static void
print_usage (void)
{
	int i;

	printf("\nOptions:\n");
	for (i = 0; i < sizeof(options)/sizeof(options[0]) && options[i].name != 0; i++) {
		printf(" %s\t%c%c\t%s\n",
		       optarghelp[i],
		       (options[i].val == 0 ? ' ' : '-'),
		       (options[i].val == 0 ? ' ' : options[i].val),
		       opthelp[i]);
	}

} /* print_usage */


/*
 * get_soctype
 */
static tegra_soctype_t
get_soctype (void) {
	ssize_t typelen;
	int fd;
	char soctype[65];
	unsigned long chipid;

	fd = open("/sys/module/tegra_fuse/parameters/tegra_chip_id", O_RDONLY);
	if (fd < 0)
		return (tegra_soctype_t)(-1);
	typelen = read(fd, soctype, sizeof(soctype)-1);
	close(fd);
	if (typelen < 0)
		return (tegra_soctype_t)(-1);
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
	return (tegra_soctype_t)(-1);

} /* get_soctype */

/*
 * get_prod_mode
 *
 * XXX just returns 0 in case of error.
 */
static unsigned long
get_prod_mode (void) {
	ssize_t modelen;
	int fd;
	char prod_mode[65];

	fd = open("/sys/module/tegra_fuse/parameters/tegra_prod_mode", O_RDONLY);
	if (fd < 0)
		return 0;
	modelen = read(fd, prod_mode, sizeof(prod_mode)-1);
	close(fd);
	if (modelen < 0)
		return 0;
	while (modelen > 0 && prod_mode[modelen-1] == '\n') modelen--;

	prod_mode[modelen] = '\0';

	return strtoul(prod_mode, NULL, 10);

} /* get_prod_mode */

/*
 * main program
 */
int
main (int argc, char * const argv[])
{
	int c, which, ret;
	char *argv0_copy = strdup(argv[0]);
	eeprom_context_t ectx;
	module_eeprom_t eeprom;
	tegra_soctype_t soctype;
	char boardrev[4];
	
	progname = basename(argv0_copy);

	for (;;) {
		c = getopt_long_only(argc, argv, shortopts, options, &which);
		if (c == -1)
			break;


		switch (c) {

		case 'h':
			print_usage();
			ret = 0;
			goto depart;
		default:
			fprintf(stderr, "Error: unrecognized option\n");
			print_usage();
			ret = 1;
			goto depart;
		}
	}

	argc -= optind;
	argv += optind;

	soctype = get_soctype();
	if ((int) soctype < 0) {
		fprintf(stderr, "Error: could not identify SoC type\n");
		ret = 1;
		goto depart;
	}

	ectx = eeprom_open_i2c(cvm_addr[soctype].busnum, cvm_addr[soctype].addr, module_type_cvm);
	if (ectx == NULL) {
		ret = errno;
		fprintf(stderr, "Error: could not open CVM EEPROM\n");
		goto depart;
	}
	if (eeprom_read(ectx, &eeprom) != 0) {
		fprintf(stderr, "Error: could not read CVM EEPROM\n");
		ret = 1;
		goto depart;
	}
	if (eeprom.partnumber_type != partnum_type_nvidia) {
		fprintf(stderr, "Error: customer part number in CVM EEPROM\n");
		ret = 1;
		goto depart;
	}
	/*
	 * Part number is 699-8bbbb-ssss-fff RRR:
	 *   bbbb = boardid
	 *   ssss = boardsku
	 *   fff  = fab
         *   RRR  = boardrev
	 */
	if (eeprom.partnumber[18] == ' ' &&
	    isprint(eeprom.partnumber[19]) &&
	    isprint(eeprom.partnumber[20]) &&
	    isprint(eeprom.partnumber[21])) {
		memcpy(boardrev, &eeprom.partnumber[19], 3);
		boardrev[3] = '\0';
	} else
		boardrev[0] = '\0';
	/*
	 * XXX punt on the chip revision for now,
	 * should be 0 for non-T194 parts, 2 for T194.
	 * Could read the chip rev from sysfs, but
	 * the value is a kernel-defined enum that
	 * could change.
	 */
	printf("%-4.4s-%-3.3s-%-4.4s-%s-%lu-%u\n",
	       &eeprom.partnumber[5],
	       &eeprom.partnumber[15],
	       &eeprom.partnumber[10],
	       boardrev,
	       get_prod_mode(),
	       (soctype == TEGRA_SOCTYPE_194 ? 2 : 0));
	ret = 0;
depart:
	eeprom_close(ectx);
	free(argv0_copy);
	return ret;

} /* main */
