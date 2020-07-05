/*
 * tegra-eeprom-tool
 *
 * Tool for working with Tegra identification EEPROMs.
 *
 * Copyright (c) 2019, 2020 Matthew Madison
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <libgen.h>
#include <histedit.h>
#include <unistd.h>
#include <ctype.h>
#include <locale.h>
#include <limits.h>
#include "eeprom.h"
#include "cvm.h"

struct context_s {
	eeprom_context_t e;
	eeprom_module_type_t mtype;
	module_eeprom_t data;
	int havedata;
	int readonly;
	int data_modified;
};
typedef struct context_s *context_t;

typedef int (*option_routine_t)(context_t ctx, int argc, char * const argv[]);

static int do_help(context_t ctx, int argc, char * const argv[]);
static int do_show(context_t ctx, int argc, char * const argv[]);
static int do_verify(context_t ctx, int argc, char * const argv[]);
static int do_get(context_t ctx, int argc, char * const argv[]);
static int do_set(context_t ctx, int argc, char * const argv[]);
static int do_write(context_t ctx, int argc, char * const argv[]);

static struct {
	const char *name;
	size_t offset;
	size_t length;
	enum {
		char_string,
		mac_address,
	} fieldtype;
	int cvm_only;
} eeprom_fields[] = {
	{ "partnumber", offsetof(module_eeprom_t, partnumber), 22, char_string },
	{ "factory-default-wifi-mac", offsetof(module_eeprom_t, factory_default_wifi_mac), 6, mac_address, 1 },
	{ "factory-default-bt-mac", offsetof(module_eeprom_t, factory_default_bt_mac), 6, mac_address, 1 },
	{ "factory-default-wifi-alt-mac", offsetof(module_eeprom_t, factory_default_wifi_alt_mac), 6, mac_address, 1 },
	{ "factory-default-ether-mac", offsetof(module_eeprom_t, factory_default_ether_mac), 6, mac_address, 1 },
	{ "asset-id", offsetof(module_eeprom_t, asset_id), 15, char_string },
	{ "vendor-wifi-mac", offsetof(module_eeprom_t, vendor_wifi_mac), 6, mac_address, 1 },
	{ "vendor-bt-mac", offsetof(module_eeprom_t, vendor_bt_mac), 6, mac_address, 1 },
	{ "vendor-ether-mac", offsetof(module_eeprom_t, vendor_ether_mac), 6, mac_address, 1 },
};
#define EEPROM_FIELD_COUNT (sizeof(eeprom_fields)/sizeof(eeprom_fields[0]))

static struct {
	const char *cmd;
	option_routine_t rtn;
	const char *help;
} commands[] = {
	{ "show",	do_show,	"show EEPROM contents" },
	{ "get",	do_get,		"get value for an EEPROM field" },
	{ "set",	do_set, 	"set a value for an EEPROM field" },
	{ "help",	do_help, 	"display extended help" },
	{ "verify",	do_verify, 	"verify EEPROM contents" },
	// commands not for use in oneshot mode follow
	{ "write",	do_write, 	"write updated EEPROM contents" },
	{ "quit",	NULL,		"exit from program" },
};
static const int non_oneshot_commands = 2;

static struct option options[] = {
	{ "device",		required_argument,	0, 'd' },
	{ "cvm",		no_argument,		0, 'c' },
	{ "help",		no_argument,		0, 'h' },
	{ 0,			0,			0, 0   }
};
static const char *shortopts = ":d:chv";

static char *optarghelp[] = {
	"--device             ",
	"--cvm                ",
	"--help               ",
	"--verbose            ",
};

static char *opthelp[] = {
	"either an I2C address (<b>-<hexaddr>) or the pathname of an EEPROM or file (REQUIRED)",
	"EEPROM is for a SoM ('cvm' type) rather than a board",
	"display this help text",
	"log informational messages to stderr"
};

static char *progname;
static char promptstr[256];
static int continuation;
static int verbose;

static uint8_t
hexdigit (int c) {
	if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	return c - '0';
}

static ssize_t
format_macaddr (char *buf, size_t bufsize, uint8_t *a)
{
	ssize_t n;
	n = snprintf(buf, bufsize-1, "%02x:%02x:%02x:%02x:%02x:%02x",
		     a[0], a[1], a[2], a[3], a[4], a[5]);
	if (n > 0)
		*(buf + n) = '\0';

	return n;

} /* format_macaddr */

static int
parse_macaddr (uint8_t *a, const char *buf)
{
	const char *cp = buf;
	int count = 0;

	while (*cp != '\0' && count < 6) {
		if (!isxdigit(*cp) || !isxdigit(*(cp+1)))
			break;
		a[count++] = (hexdigit(tolower(*cp)) << 4) | hexdigit(tolower(*(cp+1)));
		cp += 2;
		if (*cp == ':' || *cp == '-')
			cp += 1;
	}
	return (count == 6 && *cp == '\0') ? 0 : -1;

} /* parse_macaddr */

static ssize_t
format_field (context_t ctx, int i, char *strbuf, size_t bufsize)
{
	uint8_t *data = (uint8_t *) &ctx->data;
	ssize_t len = -1;

	switch (eeprom_fields[i].fieldtype) {
	case char_string:
		len = eeprom_fields[i].length;
		if (len >= bufsize)
			len = bufsize-1;
		memcpy(strbuf, data + eeprom_fields[i].offset, len);
		strbuf[len] = '\0';
		break;
	case mac_address:
		len = format_macaddr(strbuf, bufsize, data + eeprom_fields[i].offset);
		break;
	default:
		fprintf(stderr, "Internal error: unknown field type for %d\n", i);
		break;
	}
	return len;

} /* format_field */

static int
parse_fieldname (const char *s)
{
	int i;
	for (i = 0; i < EEPROM_FIELD_COUNT && strcasecmp(s, eeprom_fields[i].name) != 0; i++);
	return i >= EEPROM_FIELD_COUNT ? -1 : i;
} /* parse_fieldname */

static void
print_usage (int oneshot)
{
	int i;
	int cmdcount = sizeof(commands)/sizeof(commands[0]);

	if (oneshot) {
		cmdcount -= non_oneshot_commands;
		printf("\nUsage:\n");
		printf("\t%s <option> [<command> [<key>] [<value>]]\n\n", progname);
	}
	printf("Commands:\n");
	for (i = 0; i < cmdcount; i++)
		printf(" %s\t\t%s\n", commands[i].cmd, commands[i].help);
	if (oneshot) {
		printf("\nOptions:\n");
		for (i = 0; i < sizeof(options)/sizeof(options[0]) && options[i].name != 0; i++) {
			printf(" %s\t%c%c\t%s\n",
			       optarghelp[i],
			       (options[i].val == 0 ? ' ' : '-'),
			       (options[i].val == 0 ? ' ' : options[i].val),
			       opthelp[i]);
		}
	}

} /* print_usage */

/*
 * do_help
 *
 * Extended help that lists the valid tag names
 */
static int
do_help (context_t ctx, int argc, char * const argv[])
{

	int i;

	print_usage(0);
	printf("\nRecognized fields:\n");
	for (i = 0; i < EEPROM_FIELD_COUNT; i++)
		printf("  %s\n", eeprom_fields[i].name);
	return 0;

} /* do_help */

/*
 * do_show
 *
 * Print EEPROM contents
 */
static int
do_show (context_t ctx, int argc, char * const argv[])
{
	int i;
	char strbuf[128];

	if (!ctx->havedata && !ctx->data_modified) {
		fprintf(stderr, "Error: no valid EEPROM contents\n");
		return 1;
	}
	for (i = 0; i < EEPROM_FIELD_COUNT; i++) {
		if (ctx->mtype != module_type_cvm && eeprom_fields[i].cvm_only)
			continue;
		if (format_field(ctx, i, strbuf, sizeof(strbuf)) < 0)
			fprintf(stderr, "Error: could not format field '%s'\n", eeprom_fields[i].name);
		else
			printf("%s%s: %s\n", eeprom_fields[i].name,
			       (i == 0 ? (ctx->data.partnumber_type == partnum_type_nvidia ? "[nvidia]" : "[customer]") : ""),
			       strbuf);
	}

	return 0;

} /* do_show */

/*
 * do_get
 *
 * Get a single value
 */
static int
do_get (context_t ctx, int argc, char * const argv[])
{
	char strbuf[128];
	int i;

	if (argc < 1) {
		fprintf(stderr, "missing required argument: field-name\n");
		return 1;
	}
	i = parse_fieldname(argv[0]);
	if (i < 0) {
		fprintf(stderr, "unrecognized field name: %s\n", argv[0]);
		return 1;
	}
	if (!ctx->havedata && !ctx->data_modified) {
		fprintf(stderr, "Error: no valid EEPROM contents\n");
		return 1;
	}
	if (ctx->mtype != module_type_cvm && eeprom_fields[i].cvm_only) {
		fprintf(stderr, "Error: field not supported for this module type\n");
		return 1;
	}
	if (format_field(ctx, i, strbuf, sizeof(strbuf)) < 0) {
		fprintf(stderr, "Error: could not format field '%s'\n", eeprom_fields[i].name);
		return 1;
	}
	printf("%s%s\n", strbuf,
	       (i == 0 ? (ctx->data.partnumber_type == partnum_type_nvidia ? " [nvidia]" : " [customer]") : ""));
	return 0;

} /* do_get */


/*
 * do_set
 *
 * Set a single value
 */
static int
do_set (context_t ctx, int argc, char * const argv[])
{
	int i, valindex;
	uint8_t *data = (uint8_t *) &ctx->data;
	uint8_t addr[6];
	size_t len;

	if (argc < 2) {
		fprintf(stderr, "missing required arguments: <field-name> <value>\n");
		return 1;
	}
	i = parse_fieldname(argv[0]);
	if (i < 0) {
		fprintf(stderr, "unrecognized field name: %s\n", argv[0]);
		return 1;
	}
	if (ctx->readonly) {
		fprintf(stderr, "Error: EEPROM is read-only\n");
		return 1;
	}
	if (ctx->mtype != module_type_cvm && eeprom_fields[i].cvm_only) {
		fprintf(stderr, "Error: field not supported for this module type\n");
		return 1;
	}
	valindex = 1;
	/*
	 * partnumber also takes 'nvidia' or 'customer'
	 */
	if (i == 0) {
		if (argc < 3) {
			fprintf(stderr, "missing required arguments: <field-name> {nvidia|customer} <value>\n");
			return 1;
		}
		valindex = 2;
		len = strlen(argv[1]);
		if (strncasecmp(argv[1], "customer", len) == 0) {
			ctx->data.partnumber_type = partnum_type_customer;
		} else if (strncasecmp(argv[1], "nvidia", len) == 0) {
			ctx->data.partnumber_type = partnum_type_nvidia;
		} else {
			fprintf(stderr, "partnumber type must be either 'nvidia' or 'customer'\n");
			return 1;
		}
	}

	switch (eeprom_fields[i].fieldtype) {
	case char_string:
		len = strlen(argv[valindex]);
		if (len > eeprom_fields[i].length) {
			fprintf(stderr, "Error: value longer than field length (%zu)\n", eeprom_fields[i].length);
			return 1;
		}
		memcpy(data + eeprom_fields[i].offset, argv[valindex], len);
		while (len < eeprom_fields[i].length) {
			*(data + eeprom_fields[i].offset + len) = '\0';
			len += 1;
		}
		break;
	case mac_address:
		if (parse_macaddr(addr, argv[valindex]) < 0) {
			fprintf(stderr, "Error: could not parse MAC addresss '%s'\n", argv[valindex]);
			return 1;
		}
		memcpy(data + eeprom_fields[i].offset, addr, sizeof(addr));
		break;
	default:
		fprintf(stderr, "Internal error: unrecognized field type for '%s'\n", eeprom_fields[i].name);
		return 2;
	}

	ctx->data_modified = 1;
	return 0;

} /* do_set */

/*
 * do_verify
 *
 * Verify EEPROM contents are valid
 */
static int
do_verify (context_t ctx, int argc, char * const argv[])
{
	if (ctx->data_modified) {
		fprintf(stderr, "Error: pending changes, write before verifying\n");
		return 1;
	}
	if (!eeprom_data_valid(ctx->e)) {
		fprintf(stderr, "Verification failed: EEPROM contents not valid\n");
		return 1;
	}
	printf("Verification successful\n");
	return 0;

} /* do_verify */

/*
 * do_write
 *
 * Write EEPROM contents;
 */
static int
do_write (context_t ctx, int argc, char * const argv[])
{
	if (ctx->readonly) {
		fprintf(stderr, "Error: EEPROM is read-only\n");
		return 1;
	}
	/*
	 * havedata is set if we read in valid data;
	 * data_modified is set if we changed a field
	 *
	 * Only do a write if we either did *not* have valid data
	 * (so we're initializing to null settings), or if we
	 * changed something.
	 */
	if (ctx->havedata && !ctx->data_modified) {
		fprintf(stderr, "Error: no updates to write\n");
		return 1;
	}
	if (eeprom_write(ctx->e, &ctx->data) < 0) {
		fprintf(stderr, "Error: EEPROM write failed: %s\n", strerror(errno));
		return 1;
	}
	ctx->havedata = 1;
	ctx->data_modified = 0;
	return 0;

} /* do_write */

static char *prompt (EditLine *e)
{
	return promptstr + (continuation ? 0 : 1);
}
/*
 * command_loop
 *
 */
static int
command_loop (context_t ctx)
{
	option_routine_t dispatch;
	EditLine *el;
	History *hist;
	HistEvent ev;
	const LineInfo *li;
	Tokenizer *tok;
	char *editor;
	const char *line, **argv;
	size_t promptlen;
	int argc, alldone, llen, which, ret, n;

	setlocale(LC_CTYPE, "");
	promptlen = snprintf(promptstr, sizeof(promptstr)-2, "_%s> ", progname);
	promptstr[promptlen] = '\0';
	el = el_init(progname, stdin, stdout, stderr);

	el_set(el, EL_PROMPT, &prompt);
	editor = getenv("EDITOR");
	if (editor != NULL && strchr(editor, ' ') != NULL) {
		char *edtemp = strdup(editor);
		char *sp = strchr(edtemp, ' ');
		*sp = '\0';
		el_set(el, EL_EDITOR, edtemp);
		free(edtemp);
	} else if (editor != NULL)
		el_set(el, EL_EDITOR, editor);
	hist = history_init();
	if (hist != NULL) {
		history(hist, &ev, H_SETSIZE, 100);
		el_set(el, EL_HIST, history, hist);
	}
	el_set(el, EL_SIGNAL, 1);
	tok = tok_init(NULL);
	continuation = alldone = 0;
	while (!alldone && (line = el_gets(el, &llen)) != NULL && llen != 0) {
		li = el_line(el);
		if (!continuation && llen == 1)
			continue;
		argc = 0;
		n = tok_line(tok, li, &argc, &argv, NULL, NULL);
		if (n < 0) {
			fprintf(stderr, "internal error\n");
			continuation = 0;
			continue;
		}
		history(hist, &ev, (continuation ? H_APPEND : H_ENTER), line);
		continuation = n;
		if (continuation)
			continue;
		for (which = 0; which < sizeof(commands)/sizeof(commands[0]); which++) {
			if (strcmp(argv[0], commands[which].cmd) == 0) {
				dispatch = commands[which].rtn;
				if (dispatch == NULL)
					alldone = 1;
				break;
			}
		}
		if (which >= sizeof(commands)/sizeof(commands[0]))
			fprintf(stderr, "unrecognized command: %s\n", argv[0]);
		else if (alldone)
			break;
		else
			ret = dispatch(ctx, argc-1, (char * const *)&argv[1]);

		tok_reset(tok);
	}
	if (line == NULL && isatty(fileno(stdin)))
		printf("\n");
	el_end(el);
	tok_end(tok);
	history_end(hist);
	return ret;

} /* command_loop */

/*
 * main program
 */
int
main (int argc, char * const argv[])
{
	int c, which, ret;
	context_t ctx = NULL;
	option_routine_t dispatch = NULL;
	char *argv0_copy = strdup(argv[0]);
        char *eeprom_device = NULL;
	cvm_i2c_address_t i2c_address;
	const cvm_i2c_address_t *i2caddr;
	int use_i2c;
	ssize_t len;
	char eeprompath[PATH_MAX];
	eeprom_module_type_t mtype = module_type_normal;

	progname = basename(argv0_copy);

	for (;;) {
		c = getopt_long_only(argc, argv, shortopts, options, &which);
		if (c == -1)
			break;


		switch (c) {

		case 'h':
			print_usage(1);
			ret = 0;
			goto depart;
		case 'd':
			eeprom_device = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'c':
			mtype = module_type_cvm;
			break;
		default:
			fprintf(stderr, "Error: unrecognized option\n");
			print_usage(1);
			ret = 1;
			goto depart;
		}
	}

	argc -= optind;
	argv += optind;

	/*
	 * If no device specified, assume CVM is desired.
	 * Otherwise, if the device looks like an I2C address, use that.
	 */ 
	i2caddr = NULL;
	use_i2c = 0;
	if (eeprom_device == NULL) {
		i2caddr = cvm_i2c_address();
		if (i2caddr == NULL) {
			fprintf(stderr, "Error: no EEPROM device specified and cannot identify CVM location\n");
			print_usage(1);
			ret = 1;
			goto depart;
		}
		mtype = module_type_cvm;
	} else if (sscanf(eeprom_device, "%d-%04x", &i2c_address.busnum, &i2c_address.addr) == 2)
			i2caddr = &i2c_address;

	/*
	 * If we have an I2C address, see if there's an EEPROM driver loaded for it.
	 * If so, prefer using that rather than userland I2C calls.
	 */
	if (i2caddr != NULL) {
		len = snprintf(eeprompath, sizeof(eeprompath)-1, "/sys/bus/i2c/devices/%d-%04x/eeprom",
			       i2caddr->busnum, i2caddr->addr);
		if (len < 0) {
			fprintf(stderr, "Error: could not format path name for EEPROM\n");
			ret = 1;
			goto depart;
		}
		eeprompath[len] = '\0';
		if (access(eeprompath, F_OK) == 0)
			eeprom_device = eeprompath;
		else
			use_i2c = 1;
	}

	ctx = calloc(1, sizeof(struct context_s));
	if (ctx == NULL) {
		perror("allocating context structure");
		return errno;
	}
	if (use_i2c)
		ctx->e = eeprom_open_i2c(i2caddr->busnum, i2caddr->addr, mtype);
	else
		ctx->e = eeprom_open(eeprom_device, mtype);
	if (ctx->e == NULL) {
		perror(eeprom_device);
		free(ctx);
		return errno;
	}
	ctx->mtype = mtype;
	ctx->havedata = eeprom_read(ctx->e, &ctx->data) == 0;
	ctx->readonly = eeprom_readonly(ctx->e);

	if (argc < 1) {
		ret = command_loop(ctx);
		goto depart;
	}

	for (which = 0; which < sizeof(commands)/sizeof(commands[0])-non_oneshot_commands; which++) {
		if (strcmp(argv[0], commands[which].cmd) == 0) {
			dispatch = commands[which].rtn;
			break;
		}
	}

	if (dispatch == NULL) {
		fprintf(stderr, "Unrecognized command\n");
		ret = 1;
		goto depart;
	}

	argc -= 1;
	argv += 1;

	ret = dispatch(ctx, argc, argv);
depart:
	if (ctx != NULL) {
		if (ctx->data_modified) {
			int saveret = eeprom_write(ctx->e, &ctx->data);
			if (saveret != 0) {
				fprintf(stderr, "Error: could not write EEPROM data\n");
				if (ret == 0)
					ret = saveret;
			}
		}
		eeprom_close(ctx->e);
		free(ctx);
	}
	free(argv0_copy);
	return ret;

} /* main */
