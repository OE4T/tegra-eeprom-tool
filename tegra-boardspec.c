/*
 * tegra-boardspec
 *
 * Extracts board information from the CVM EEPROM.
 *
 * Copyright (c) 2020 Matthew Madison
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <libgen.h>
#include "boardspec.h"

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
 * main program
 */
int
main (int argc, char * const argv[])
{
	int c, which, ret, len;
	char specbuf[128];
	char *argv0_copy = strdup(argv[0]);
	
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

	len = tegra_boardspec(specbuf, sizeof(specbuf)-1);
	if (len < 0) {
		perror("tegra_boardspec");
		ret = 1;
	}
	specbuf[len] = '\0';
	printf("%s\n", specbuf);
	ret = 0;
depart:
	free(argv0_copy);
	return ret;

} /* main */
