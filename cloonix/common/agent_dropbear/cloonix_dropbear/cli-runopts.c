/*
 * Dropbear - a SSH2 server
 * Copyright (c) 2002,2003 Matt Johnston
*/
#include "includes.h"
#include "runopts.h"
#include "buffer.h"
#include "dbutil.h"
#include "algo.h"
#include "list.h"
#include "io_clownix.h"

cli_runopts cli_opts;


static void parse_hostname(char* orighostarg) 
{
  char *userhostarg = NULL;
  if (!orighostarg)
    KOUT(" ");
  userhostarg = m_strdup(orighostarg);
  cli_opts.vmname = strchr(userhostarg, '@');
  if (cli_opts.vmname == NULL) 
    {
    cli_opts.vmname = userhostarg;
    } 
  else 
    {
    cli_opts.vmname[0] = 0;
    cli_opts.vmname++;
    }
  if (cli_opts.vmname[0] == 0)
    KOUT("Bad hostname");
}



void cli_getopts(int argc, char ** argv) 
{
  int i, j;
  char ** next = 0;
  unsigned int cmdlen;
  char *host_arg = NULL;

  cli_opts.progname = argv[0];
  cli_opts.cloonix_doors = argv[1];
  argc--;
  argv++;
  cli_opts.cloonix_password = argv[1];
  argc--;
  argv++;
  cli_opts.vmname = NULL;
  cli_opts.cmd = NULL;
  cli_opts.wantpty = 9;
  opts.recv_window = DEFAULT_RECV_WINDOW;

  for (i = 1; i < argc; i++) 
    {
    if (next) 
      {
      *next = argv[i];
      if (*next == NULL) 
        KOUT("Invalid null argument");
      next = NULL;
      continue;
      }
    if (argv[i][0] == '-') 
      {
      switch (argv[i][1]) 
        {
        case 't':
          cli_opts.wantpty = 1;
        break;
        default:
          KOUT("unknown argument '%s'\n", argv[i]);
        }
      if (next && strlen(argv[i]) > 2) 
        {
        *next = &argv[i][2];
        next = NULL;
        }
      } 
    else 
      {
      if (host_arg == NULL) 
        {
        host_arg = argv[i];
        }
      else 
        {
        cmdlen = 0;
        for (j = i; j < argc; j++) 
	  cmdlen += strlen(argv[j]) + 1; 
        cli_opts.cmd = (char*)m_malloc(cmdlen);
        memset(cli_opts.cmd, 0, cmdlen);
        for (j = i; j < argc; j++) 
          {
	  strlcat(cli_opts.cmd, argv[j], cmdlen);
	  strlcat(cli_opts.cmd, " ", cmdlen);
          }
        break;
        }
      }
    }
  if (cli_opts.wantpty == 9) 
    {
    if (cli_opts.cmd == NULL) 
      cli_opts.wantpty = 1;
    else 
      cli_opts.wantpty = 0;
    }
  parse_hostname(host_arg);
}




