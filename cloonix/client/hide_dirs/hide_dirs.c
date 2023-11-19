/*
 * Inspired from (c) 2023 Richard Weinberger <richard@sigma-star.at>
 * This file is public domain.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_PATH_LEN 300
static FILE *g_log_cmd;

/****************************************************************************/
static void set_env_global_cloonix(void)
{
  setenv("PATH",  "/usr/libexec/cloonix/common:"
                  "/usr/libexec/cloonix/client:"
                  "/usr/libexec/cloonix/server", 1);
  setenv("FONTCONFIG_FILE",
  "/usr/libexec/cloonix/common/etc/fonts/fonts.conf", 1);
  setenv("XDG_CONFIG_HOME",
  "/usr/libexec/cloonix/common/share", 1);
  setenv("XDG_DATA_HOME",
  "/usr/libexec/cloonix/common/share", 1);
  setenv("XDG_DATA_DIRS",
  "/usr/libexec/cloonix/common/share", 1);
  setenv("GTK_DATA_PREFIX",
  "/usr/libexec/cloonix/common/share", 1);
  setenv("GTK_EXE_PREFIX",
  "/usr/libexec/cloonix/common", 1);
  setenv("QT_PLUGIN_PATH",
  "/usr/libexec/cloonix/common/lib/qt6/plugins", 1);
  setenv("GTK_IM_MODULE_FILE",
  "/usr/libexec/cloonix/common/lib/gtk-3.0/3.0.0/immodules.cache", 1);
  setenv("GDK_PIXBUF_MODULE_FILE",
  "/usr/libexec/cloonix/common/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache", 1);
  setenv("GST_PLUGIN_SYSTEM_PATH",
  "/usr/libexec/cloonix/common/lib/gstreamer-1.0", 1);
  setenv("GST_PLUGIN_SCANNER",
  "/usr/libexec/cloonix/common/gst-plugin-scanner", 1);
  setenv("NO_AT_BRIDGE", "1", 1);
  setenv("PULSE_CLIENTCONFIG",
  "/usr/libexec/cloonix/common/etc/pulse/client.conf", 1);
  setenv("LIBGL_ALWAYS_INDIRECT", "1", 1);
  setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);
  setenv("LIBGL_DRI3_DISABLE", "1", 1);
  setenv("QT_X11_NO_MITSHM", "1", 1);
  setenv("QT_XCB_NO_MITSHM", "1", 1);
   setenv("LC_CTYPE", "en_US.UTF-8", 1);
   setenv("LC_ALL", "en_US.UTF-8", 1);
   setenv("LANG", "en_US.UTF-8", 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_env_ice_global_cloonix(void)
{
  setenv("GST_PLUGIN_SYSTEM_PATH",
  "/usr/libexec/cloonix/common/lib/gstreamer-1.0", 1);
  setenv("GST_PLUGIN_SCANNER",
  "/usr/libexec/cloonix/common/gst-plugin-scanner", 1);
}
/*--------------------------------------------------------------------------*/

static void write_proc_self(const char *file, const char *content)
{
  size_t len = strlen(content) + 1;
  char *path;
  int fd;
  assert(asprintf(&path, "/proc/self/%s", file) != -1);
  fd = open(path, O_RDWR | O_CLOEXEC);
  free(path);
  assert(fd >= 0);
  assert(write(fd, content, len) == len);
  close(fd);
}

static void update_uidgid_map(uid_t from, uid_t to, bool is_user)
{
  char *map_content;
  assert(asprintf(&map_content, "%u %u 1\n", from, to) != -1);
  if (is_user)
    write_proc_self("uid_map", map_content);
  else
    write_proc_self("gid_map", map_content);
  free(map_content);
}

static void deny_setgroups(void)
{
  write_proc_self("setgroups", "deny\n");
}

static void become_uid0(uid_t orig_uid, gid_t orig_gid)
{
  deny_setgroups();
  update_uidgid_map(0, orig_gid, false);
  update_uidgid_map(0, orig_uid, true);
  assert(setuid(0) == 0);
  assert(setgid(0) == 0);
}

static void become_orig(uid_t orig_uid, gid_t orig_gid)
{
  update_uidgid_map(orig_gid, 0, false);
  update_uidgid_map(orig_uid, 0, true);
  assert(setuid(orig_uid) == 0);
  assert(setgid(orig_gid) == 0);
}

static void hide_dir_if_necessary(const char *input)
{
  struct stat sb;
  if (lstat(input, &sb) == 0)
    {
    if ((sb.st_mode & S_IFMT) == S_IFLNK)
      {
      }
    else if ((sb.st_mode & S_IFMT) == S_IFDIR)
      {
      assert(mount("tmpfs", input, "tmpfs", 0, NULL) == 0);
      }
    else
      printf("!!!!!!!!!!!!!!!!! UNEXPECTED %s !!!!!!!!!!\n", input);
    }
}


static void setup_mounts(void)
{
  char *curdir = get_current_dir_name();
  assert(curdir);
  assert(mount("none", "/", NULL, MS_REC | MS_SLAVE, NULL) == 0);
  hide_dir_if_necessary("/etc");
  hide_dir_if_necessary("/bin");
  hide_dir_if_necessary("/sbin");
  hide_dir_if_necessary("/lib");
  hide_dir_if_necessary("/libx32");
  hide_dir_if_necessary("/lib32");
  hide_dir_if_necessary("/lib64");
  mkdir("/var/lib/cloonix", 0777);
  mkdir("/var/lib/cloonix/libexec", 0777);
  assert(mount("/usr/libexec/cloonix", "/var/lib/cloonix/libexec",
               NULL, MS_BIND, NULL) == 0);
  assert(mount("tmpfs", "/usr", "tmpfs", 0, NULL) == 0);
  mkdir("/usr/share", 0777);
  mkdir("/usr/libexec", 0777);
  mkdir("/usr/libexec/cloonix", 0777);
  assert(mount("/var/lib/cloonix/libexec", "/usr/libexec/cloonix",
               NULL, MS_BIND, NULL) == 0);
  assert(mount("/var/lib/cloonix/libexec/common/share",
               "/usr/share", NULL, MS_BIND, NULL) == 0);
  assert(mount("/var/lib/cloonix/libexec/common/etc",
               "/etc", NULL, MS_BIND, NULL) == 0);
  mkdir("/usr/bin", 0777);
  assert(mount("/var/lib/cloonix/libexec/common",
               "/usr/bin", NULL, MS_BIND, NULL) == 0);
  mkdir("/usr/lib", 0777);
  mkdir("/usr/lib/locale", 0777);
  system("cp -f /var/lib/cloonix/libexec/common/locale-archive "
                "/usr/lib/locale");
  chdir("/");
  chdir(curdir);
  free(curdir);
}

/****************************************************************************/
static int get_ip_pass_port(FILE *fh, char *ip, char *pass, int *port)
{
  int result = -1;
  char line[100];
  if (!fgets(line, 100, fh))
    printf("ERROR1 get_ip_pass_port\n"); 
  else if (sscanf(line, "  cloonix_ip %s", ip) != 1)
    printf("ERROR2 get_ip_pass_port\n"); 
  else if (!fgets(line, 100, fh))
    printf("ERROR3 get_ip_pass_port\n"); 
  else if (sscanf(line, "  cloonix_port %d", port) != 1)
    printf("ERROR4 get_ip_pass_port\n"); 
  else if (!fgets(line, 100, fh))
    printf("ERROR5 get_ip_pass_port\n"); 
  else if (sscanf(line, "  cloonix_passwd %s", pass) != 1)
    printf("ERROR6 get_ip_pass_port\n"); 
  else
    result = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int check_net_name(char *cnf, char *name,
                          char *ip, char *pass, int *port)
{
  int result = 0;
  FILE *fh;
  char line[100];
  char nm[80];
  fh = fopen(cnf, "r");
  if (fh == NULL) 
    {
    printf("ERROR %s\n", cnf);
    exit(-1);
    }
  else
    {
    while (fgets(line, 100, fh) != NULL)
      {
      if (sscanf(line, "CLOONIX_NET: %s {", nm) == 1)
        {
        if (!strcmp(nm, name))  
          {
          if (get_ip_pass_port(fh, ip, pass, port))
            {
            printf("ERROR %s\n", line);
            exit(-1);
            }
          result = 1;
          break;
          }
        }
      }
    fclose(fh);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void process_ocp(int argc, char **argv, char **new_argv,
                 char *ip, int port, char *passwd)
{
  static char sock[200];
  static char dist[300];
  char ocp_param[200];
  if (argc != 9)
    {
    printf("ERROR81 PARAM NUMBER\n");
    exit(-1);
    }
  sprintf(sock, "/var/lib/cloonix/%s/nat", argv[2]);
  mkdir(sock, 0777);
  sprintf(ocp_param,
          "%s=%d=%s=nat_%s@user=admin=ip=%s=port=22=cloonix_info_end",
          ip, port, passwd, argv[3], argv[4]);
  new_argv[0] = "/usr/libexec/cloonix/client/cloonix-u2i-scp";
  new_argv[1] = sock;
  new_argv[2] = "-i";
  new_argv[3] = "/tmp/cloonix_private_id_rsa";
  if (!strcmp(argv[7], "y"))
    {
    new_argv[4] = "-r";
    if (!strcmp(argv[8], "dist"))
      {
      sprintf(dist, "%s:%s", ocp_param, argv[5]);
      new_argv[5] = dist;
      new_argv[6] = argv[6];
      }
    else if (!strcmp(argv[8], "loc"))
      {
      sprintf(dist, "%s:%s", ocp_param, argv[6]);
      new_argv[5] = argv[5];
      new_argv[6] = dist;
      }
    else
      {
      printf("ERROR81 PARAM NUMBER\n");
      exit(-1);
      }
    }
  else if (!strcmp(argv[7], "n"))
    {
    if (!strcmp(argv[8], "dist"))
      {
      sprintf(dist, "%s:%s", ocp_param, argv[5]);
      new_argv[4] = dist;
      new_argv[5] = argv[6];
      }
    else if (!strcmp(argv[8], "loc"))
      {
      sprintf(dist, "%s:%s", ocp_param, argv[6]);
      new_argv[4] = argv[5];
      new_argv[5] = dist;
      }
    else
      {
      printf("ERROR82 PARAM NUMBER\n");
      exit(-1);
      }
    }
  else
    {
    printf("ERROR8 PARAM NUMBER\n");
    exit(-1);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void process_osh(int argc, char **argv, char **new_argv,
                 char *ip, int port, char *passwd)
{
  static char sock[200];
  static char ocp_param[200];
  int i;
  if (argc < 5)
    {
    printf("ERROR91 PARAM NUMBER\n");
    exit(-1);
    }
  sprintf(sock, "/var/lib/cloonix/%s/nat", argv[2]);
  mkdir(sock, 0777); 
  sprintf(ocp_param,
          "%s=%d=%s=nat_%s@user=admin=ip=%s=port=22=cloonix_info_end",
          ip, port, passwd, argv[3], argv[4]);
  new_argv[0] = "/usr/libexec/cloonix/client/cloonix-u2i-ssh";
  new_argv[1] = sock;
  new_argv[2] = "-i";
  new_argv[3] = "/tmp/cloonix_private_id_rsa";
  new_argv[4] = ocp_param;
  for (i=0; i<20; i++)
    {
    if (argc > 5+i)
      new_argv[5+i] = argv[5+i];
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void log_ascii_cmd(char *argv[])
{
  int i;
  char result[3*MAX_PATH_LEN];
  if (g_log_cmd != NULL)
    {
    memset(result, 0, 3*MAX_PATH_LEN);
    for (i=0;  (argv[i] != NULL); i++)
      {
      strcat(result, argv[i]);
      if (strlen(result) >= 2*MAX_PATH_LEN)
        break;
      strcat(result, " ");
      }
    fprintf(g_log_cmd, "%s\n", result);
    fflush(g_log_cmd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void init_log_cmd(char *network_name)
{
  char log_path[MAX_PATH_LEN];
  memset(log_path, 0, MAX_PATH_LEN);
  snprintf(log_path, MAX_PATH_LEN-1,
  "/var/lib/cloonix/%s/log/debug_cmd.log", network_name);
  g_log_cmd = fopen(log_path, "a");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{
  int fd;
  char passwd[80];
  char ip[80];
  char ipport[100];
  char title[120];
  char sock[200];
  char ocp_param[300];
  int  i, port, pid;
  char *cfg="/usr/libexec/cloonix/common/etc/cloonix.cfg";
  char *new_argv[30];
  uid_t my_uid = getuid();
  gid_t my_gid = getgid();
  g_log_cmd = NULL;
  memset(new_argv, 0, 30*sizeof(char *));
  if (argc < 3)
    {
/*CLOONIX_LSH-----------------------*/
    if (!strcmp("lsh", argv[1]))
      {     
      new_argv[0] = "/usr/libexec/cloonix/common/sh";
      }
    else if (!strcmp("cli", argv[1]))
      {
      new_argv[0] = "/usr/libexec/cloonix/client/cloonix-ctrl";
      new_argv[1] = "/usr/libexec/cloonix/common/etc/cloonix.cfg";
      }
    else
      {
      printf("ERROR1 PARAMS\n");
      exit(-1);
      }
    }
  else if (!check_net_name(cfg, argv[2], ip, passwd, &port))
    {
    printf("ERROR3 PARAM %s\n", argv[2]);
    exit(-1);
    }
  if (argc >= 3)
    {
    init_log_cmd(argv[2]);
    }
  if (!strcmp("lsh", argv[1]))
    {
    }
/*CLOONIX_DSH-----------------------*/
  else if (!strcmp("dsh", argv[1]))
    {
    new_argv[0] = "/usr/libexec/cloonix/client/cloonix-xwycli";
    new_argv[1] = "/usr/libexec/cloonix/common/etc/cloonix.cfg";
    new_argv[2] = argv[2];
    new_argv[3] = "-cmd";
    new_argv[4] = "/usr/libexec/cloonix/common/sh";
    }
/*CLOONIX_CLI-----------------------*/
  else if (!strcmp("cli", argv[1]))
    {
    if (argc < 4)
      {
      new_argv[0] = "/usr/libexec/cloonix/client/cloonix-ctrl";
      new_argv[1] = "/usr/libexec/cloonix/common/etc/cloonix.cfg";
      }
    else
      {
      new_argv[0] = "/usr/libexec/cloonix/client/cloonix-ctrl";
      new_argv[1] = "/usr/libexec/cloonix/common/etc/cloonix.cfg";
      new_argv[2] = argv[2];
      new_argv[3] = argv[3];
      for (i=0; i<20; i++)
        {
        if (argc > 4+i)
          new_argv[4+i] = argv[4+i];
        }
      }
    }
/*CLOONIX_GUI-----------------------*/
  else if (!strcmp("gui", argv[1]))
    {
    new_argv[0] = "/usr/libexec/cloonix/client/cloonix-gui";
    new_argv[1] = "/usr/libexec/cloonix/common/etc/cloonix.cfg";
    new_argv[2] = argv[2];
    }
/*CLOONIX_SCP-----------------------*/
  else if (!strcmp("scp", argv[1]))
    {
    if ((argc != 5) && (argc != 6))
      {
      printf("ERROR5 PARAM NUMBER\n");
      exit(-1);
      }
    sprintf(ipport, "%s:%d", ip, port);
    new_argv[0] = "/usr/libexec/cloonix/client/cloonix-dropbear-scp";
    new_argv[1] = ipport;
    new_argv[2] = passwd;
    new_argv[3] = argv[3];
    new_argv[4] = argv[4];
    if (argc == 6)
      new_argv[5] = argv[5];
    }
/*CLOONIX_SSH-----------------------*/
  else if (!strcmp("ssh", argv[1]))
    {
    if ((argc != 4) && (argc != 5))
      {
      printf("ERROR5 PARAM NUMBER\n");
      exit(-1);
      }
    sprintf(ipport, "%s:%d", ip, port);
    new_argv[0] = "/usr/libexec/cloonix/client/cloonix-dropbear-ssh";
    new_argv[1] = ipport;
    new_argv[2] = passwd;
    new_argv[3] = argv[3];
    if (argc == 5)
      new_argv[4] = argv[4];
    }
/*CLOONIX_ICE-----------------------*/
  else if (!strcmp("ice", argv[1]))
    {
    if (argc != 5)
      {
      printf("ERROR6 PARAM NUMBER\n");
      exit(-1);
      }
    sprintf(title, "--title=%s/%s", argv[2], argv[3]);
    sprintf(ipport, "%s:%d", ip, port);
    sprintf(sock, "/var/lib/cloonix/%s/vm/vm%s/spice_sock", argv[2], argv[4]);
    new_argv[0] = "/usr/libexec/cloonix/client/cloonix-spicy";
    new_argv[1] = title;
    new_argv[2] = "-d";
    new_argv[3] = ipport;
    new_argv[4] = "-c";
    new_argv[5] = sock;
    new_argv[6] = "-w";
    new_argv[7] = passwd;
    }
/*CLOONIX_WSK-----------------------*/
  else if (!strcmp("wsk", argv[1]))
    {
    if (argc != 5)
      {
      printf("ERROR7 PARAM NUMBER\n");
      exit(-1);
      }
    sprintf(sock, "/var/lib/cloonix/%s/snf/%s_%s", argv[2], argv[3], argv[4]);
    new_argv[0] = "/usr/libexec/cloonix/client/cloonix-xwycli";
    new_argv[1] = "/usr/libexec/cloonix/common/etc/cloonix.cfg";
    new_argv[2] = argv[2];
    new_argv[3] = "-dae";
    new_argv[4] = "/usr/libexec/cloonix/server/cloonix-wireshark";
    new_argv[5] = "-o capture.no_interface_load:TRUE -o gui.ask_unsaved:FALSE";
    new_argv[6] = "-k";
    new_argv[7] = "-i";
    new_argv[8] = sock;
    }
/*CLOONIX_OVS-----------------------*/
  else if (!strcmp("ovs", argv[1]))
    {
    sprintf(sock, "--db=unix:/var/lib/cloonix/%s/ovsdb_server.sock", argv[2]);
    new_argv[0] = "/usr/libexec/cloonix/client/cloonix-xwycli";
    new_argv[1] = "/usr/libexec/cloonix/common/etc/cloonix.cfg";
    new_argv[2] = argv[2];
    new_argv[3] = "-cmd";
    new_argv[4] = "/usr/libexec/cloonix/server/cloonix-ovs-vsctl";
    new_argv[5] = sock;
    for (i=0; i<10; i++)
      {
      if (argc > 3+i)
        new_argv[6+i] = argv[3+i];
      }
    }
/*CLOONIX_OCP-----------------------*/
  else if (!strcmp("ocp", argv[1]))
    {
    process_ocp(argc, argv, new_argv, ip, port, passwd);
    }
/*CLOONIX_OSH-----------------------*/
  else if (!strcmp("osh", argv[1]))
    {
    process_osh(argc, argv, new_argv, ip, port, passwd);
    }
  else
    {
    printf("ERROR10 PARAM %s\n", argv[1]);
    exit(-1);
    }
/*------------------------------------------------------------------------*/
  if ((!strcmp("osh", argv[1])) || (!strcmp("ocp", argv[1])))
    {
    log_ascii_cmd(new_argv);
    execv(new_argv[0], new_argv);
    }
  else if (!strcmp("ice", argv[1]))
    {
    pid = fork();
    if (pid < 0)
      {
      printf("ERROR9 PARAM %s\n", argv[1]);
      exit(-1);
      }
    else if (pid == 0)
      {
      set_env_ice_global_cloonix();
      log_ascii_cmd(new_argv);
      execv(new_argv[0], new_argv);
      }
    }
  else if (!strcmp("wsk", argv[1]))
    {
    pid = fork();
    if (pid < 0)
      {
      printf("ERROR10 PARAM %s\n", argv[1]);
      exit(-1);
      }
    else if (pid == 0)
      {
      assert(unshare(CLONE_NEWNS | CLONE_NEWUSER) == 0);
      become_uid0(my_uid, my_gid);
      setup_mounts();
      assert(unshare(CLONE_NEWUSER) == 0);
      become_orig(my_uid, my_gid);
      set_env_global_cloonix();
      if (!strcmp("gui", argv[1]))
        {
        fd = open("/dev/null", O_WRONLY);
        if (fd < 0)
          {
          printf("ERROR11 PARAM %s\n", argv[1]);
          exit(-1);
          }
        if (dup2(fd,STDOUT_FILENO) < 0)
          {
          printf("ERROR11 PARAM %s\n", argv[1]);
          exit(-1);
          }
        if (dup2(fd,STDERR_FILENO) < 0)
          {
          printf("ERROR11 PARAM %s\n", argv[1]);
          exit(-1);
          }
        }
      log_ascii_cmd(new_argv);
      execv(new_argv[0], new_argv);
      }
    }
  else
    {
    assert(unshare(CLONE_NEWNS | CLONE_NEWUSER) == 0);
    become_uid0(my_uid, my_gid);
    setup_mounts();
    assert(unshare(CLONE_NEWUSER) == 0);
    become_orig(my_uid, my_gid);
    set_env_global_cloonix();
    log_ascii_cmd(new_argv);
    execv(new_argv[0], new_argv);
    }
}
