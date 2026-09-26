/* dtach - Copyright (C) 2004-2016 Ned T. Crigler  SPDX-License-Identifier: GPL-2.0-or-later */
#include "dtach.h"

/* Make sure the binary has a copyright. */
const char copyright[] =
	"cloonix-dtach, from dtach 0.9 (C)Copyright 2004-2016 Ned T. Crigler";

/* argv[0] from the program */
char *progname;
/* The name of the passed in socket. */
char *sockname;
/* The character used for detaching. Defaults to '^\' */
int detach_char = '\\' - 64;
/* 1 if we should not interpret the suspend character. */
int no_suspend;

/* Original terminal settings: the master inits the pty with them, the attacher restores them. */
struct termios orig_term;
int dont_have_tty;

static void
usage(void)
{
	printf(
		"cloonix-dtach\n"
		"Usage: cloonix-dtach -n <socket> <command...>\n"
		"       cloonix-dtach -a <socket> [-E] [-z]\n"
		"Modes:\n"
		"  -n\t\tCreate a socket and run the command detached.\n"
		"  -a\t\tAttach to the socket.\n"
		"Options for -a:\n"
		"  -E\t\tDisable the detach character.\n"
		"  -z\t\tDisable processing of the suspend key.\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	int mode = 0;

	/* Save the program name */
	progname = argv[0];
	++argv; --argc;

	if (argc >= 1 && **argv == '-')
	{
		mode = argv[0][1];
		if (mode != 'a' && mode != 'n')
		{
			printf("%s: invalid mode '-%c'\n", progname, mode);
			usage();
		}
	}
	if (!mode)
	{
		printf("%s: no mode was specified.\n", progname);
		usage();
	}
	++argv; --argc;

	if (argc < 1)
	{
		printf("%s: no socket was specified.\n", progname);
		usage();
	}
	sockname = *argv;
	++argv; --argc;

	while (argc >= 1 && **argv == '-')
	{
		char *p;

		for (p = argv[0] + 1; *p; ++p)
		{
			if (*p == 'E')
				detach_char = -1;
			else if (*p == 'z')
				no_suspend = 1;
			else
			{
				printf("%s: invalid option '-%c'\n", progname, *p);
				usage();
			}
		}
		++argv; --argc;
	}

	if (mode == 'n' && argc < 1)
	{
		printf("%s: no command was specified.\n", progname);
		usage();
	}
	if (mode == 'a' && argc > 0)
	{
		printf("%s: -a takes no command.\n", progname);
		usage();
	}

	/* Save the original terminal settings. */
	if (tcgetattr(0, &orig_term) < 0)
	{
		memset(&orig_term, 0, sizeof(struct termios));
		dont_have_tty = 1;
	}

	if (mode == 'a')
	{
		if (dont_have_tty)
		{
			printf("%s: attaching requires a terminal.\n", progname);
			return 1;
		}
		return attach_main();
	}
	return master_main(argv);
}
