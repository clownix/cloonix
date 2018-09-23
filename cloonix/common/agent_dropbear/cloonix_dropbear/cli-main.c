/*
 * Stolen for cloonix from:
 * Dropbear - a SSH2 server
 * SSH client implementation
*/

#include "includes.h"
#include "dbutil.h"
#include "runopts.h"
#include "session.h"
#include "io_clownix.h"

static char g_cloonix_tree[MAX_PATH_LEN];
static char g_current_directory[MAX_PATH_LEN];

int cloonix_connect_remote(char *cloonix_tree,
                           char *cloonix_doors, 
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
static void init_local_cloonix_bin_path(char *curdir, char *callbin)
{
  char path[MAX_PATH_LEN];
  char *ptr;
  memset(g_cloonix_tree, 0, MAX_PATH_LEN);
  memset(path, 0, MAX_PATH_LEN);
  if (callbin[0] == '/')
    snprintf(path, MAX_PATH_LEN-1, "%s", callbin);
  else
    snprintf(path, MAX_PATH_LEN-1, "%s/%s", curdir, callbin);

  ptr = strrchr(path, '/');
  if (!ptr)
    KOUT("%s", path);
  *ptr = 0;
  ptr = strrchr(path, '/');
  if (!ptr)
    KOUT("%s", path);
  *ptr = 0;
  ptr = strrchr(path, '/');
  if (!ptr)
    KOUT("%s", path);
  *ptr = 0;
  ptr = strrchr(path, '/');
  if (!ptr)
    KOUT("%s", path);
  *ptr = 0;
  strncpy(g_cloonix_tree, path, MAX_PATH_LEN-1);
  snprintf(path, MAX_PATH_LEN-1,
           "%s/common/agent_dropbear/agent_bin/dropbear_cloonix_ssh", 
            g_cloonix_tree);
  if (access(path, X_OK))
    KOUT("%s", path);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int main(int argc, char ** argv)
{
  memset(g_current_directory, 0, MAX_PATH_LEN);
  if (!getcwd(g_current_directory, MAX_PATH_LEN-1))
    KOUT(" ");
  init_local_cloonix_bin_path(g_current_directory, argv[0]);
  cli_getopts(argc, argv);
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    KOUT("signal() error");
  cloonix_connect_remote(g_cloonix_tree, cli_opts.cloonix_doors, 
                         cli_opts.vmname,
                         cli_opts.cloonix_password);
  cloonix_session_loop();
  return -1;
}
/*--------------------------------------------------------------------------*/

