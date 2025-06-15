/*
 * Stolen for cloon from:
 * Dropbear - a SSH2 server
 * SSH client implementation
*/

#include "includes.h"
#include "dbutil.h"
#include "runopts.h"
#include "session.h"
#include "io_clownix.h"

void x11_init(void);

static char g_current_directory[MAX_PATH_LEN];

int cloonix_connect_remote(char *cloonix_doors, 
                           char *vmname,
                           char *password);
void cloonix_session_loop(void);


int main_i_run_in_kvm(void)
{
  return 0;
}

char *main_cloonix_tree_dir(void)
{
  return NULL;
}


/****************************************************************************/
int main(int argc, char ** argv)
{
  x11_init();
  memset(g_current_directory, 0, MAX_PATH_LEN);
  if (!getcwd(g_current_directory, MAX_PATH_LEN-1))
    KOUT(" ");
  if (access("/usr/libexec/cloonix/cloonfs/bin/cloonix-dropbear-ssh", X_OK))
    KOUT("/usr/libexec/cloonix/cloonfs/bin/cloonix-dropbear-ssh");
  cli_getopts(argc, argv);
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    KOUT("signal() error");
  cloonix_connect_remote(cli_opts.cloonix_doors, 
                         cli_opts.vmname,
                         cli_opts.cloonix_password);
  cloonix_session_loop();
  return -1;
}
/*--------------------------------------------------------------------------*/

