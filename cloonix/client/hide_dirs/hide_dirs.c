/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
/* Inspired from (c) 2023 Richard Weinberger <richard@sigma-star.at>         */
/*****************************************************************************/
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
#include <sys/wait.h>
#include <syslog.h>

#define MAX_NARGS 40
#define MAX_NAME_LEN 60
#define MAX_PATH_LEN 300
#define PROCESS_STACK 500*1024

#define KOUT(format, a...)                               \
 do {                                                    \
    printf("ERROR KILL line:%d " format "\n\n", __LINE__, ## a); \
    syslog(LOG_ERR | LOG_USER, "ERROR KILL line:%d " format "\n\n", __LINE__, ## a); \
    exit(-1);                                            \
    } while (0)

#define KERR(format, a...)                               \
 do {                                                    \
    printf("WARNING line:%d " format "\n\n", __LINE__, ## a); \
    syslog(LOG_ERR | LOG_USER, "WARNING line:%d " format "\n\n", __LINE__, ## a); \
    } while (0)



static char g_xauthority[MAX_PATH_LEN];
static uid_t g_my_uid;
static gid_t g_my_gid;
static FILE *g_log_cmd;

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void deny_setgroups(void)
{ 
  write_proc_self("setgroups", "deny\n");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void become_uid0(uid_t orig_uid, gid_t orig_gid)
{
  deny_setgroups();
  update_uidgid_map(0, orig_gid, false);
  update_uidgid_map(0, orig_uid, true);
  assert(setuid(0) == 0);
  assert(setgid(0) == 0);
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void become_orig(uid_t orig_uid, gid_t orig_gid)
{
  update_uidgid_map(orig_gid, 0, false);
  update_uidgid_map(orig_uid, 0, true);
  assert(setuid(orig_uid) == 0);
  assert(setgid(orig_gid) == 0);
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
  hide_dir_if_necessary("/usr");

  mkdir("/usr/bin", 0777);
  mkdir("/usr/tmp", 0777);
  mkdir("/usr/share", 0777);
  mkdir("/usr/share/i18n", 0777);
  mkdir("/usr/libexec", 0777);
  mkdir("/usr/libexec/cloonix", 0777);
  mkdir("/usr/lib", 0777);
  mkdir("/usr/lib/locale", 0777);
  mkdir("/usr/lib/x86_64-linux-gnu", 0777);
  mkdir("/usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0", 0777);
  mkdir("/usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/2.10.0", 0777);
  mkdir("/usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders", 0777);

  assert(mount("/var/lib/cloonix/libexec", "/usr/libexec/cloonix",
               NULL, MS_BIND, NULL) == 0);
  assert(mount("/var/lib/cloonix/libexec/common/share",
               "/usr/share", NULL, MS_BIND, NULL) == 0);
  assert(mount("/var/lib/cloonix/libexec/common/etc",
               "/etc", NULL, MS_BIND, NULL) == 0);
  assert(mount("/var/lib/cloonix/libexec/common",
               "/usr/bin", NULL, MS_BIND, NULL) == 0);
  assert(mount("/usr/libexec/cloonix/common/lib/gdk-pixbuf-2.0/2.10.0/loaders",
               "/usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders",
               NULL, MS_BIND, NULL) == 0);
  assert(mount("/usr/libexec/cloonix/common/share/i18n", "/usr/share/i18n",
               NULL, MS_BIND, NULL) == 0);
  assert(mount("/usr/libexec/cloonix/common/localedir", "/usr/lib/locale",
               NULL, MS_BIND, NULL) == 0);

  chdir("/");
  chdir(curdir);
  free(curdir);
}
/*--------------------------------------------------------------------------*/

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
   setenv("XAUTHORITY", g_xauthority, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*
# The following private id_rsa corresponds to the id_rsa.pub that
# is used in file step1_make_preconf_iso.sh for the cisco making.
*/
/****************************************************************************/
static void create_cloonix_private_id_rsa(void)
{
  size_t len, wlen;
  FILE *fh;
  char *rsa =
  "-----BEGIN RSA PRIVATE KEY-----\n"
  "MIIEowIBAAKCAQEAxkepoiuYZD/HnpXL4raEarJi7EsMFkr5NoC/cv9SF8CPweIF\n"
  "JM66qe0CUd9Jw97Ov9ulTLPnoUQYFbjb/abOa7etijRx3vsvahgQSjYpOBXCngkd\n"
  "zooAvWymLYsBsZQo0QNtXdagP0lmti5eEX+/uCv/+IWGFx7tBT8UF8s58DruDRpm\n"
  "dwzGgh/fr2FwOklxjQO71gkIzd/fpQXevsj93CsUH3UzTrvTn8BrfrKABAAeH2Qd\n"
  "1yxTHRPcf9syEzMMA8YtwCPPO8vqobMt1tH6Yrl/c7DkxqCftWjPVx0mMJ/SHBOQ\n"
  "P4NMVr+fE6R0Y1WuWVwojs86uLAtOFBl5i+VmwIDAQABAoIBAAlCzZyCdsKv6+3v\n"
  "Ry+WoMavAEnTE4RzCgLOrqJ7ZGUxnEVM/jqC4VsQc9xJFpPsczGo26aifH4exRU2\n"
  "pifJw7hqQtPCsVLd3pARAanFr9UrxwREnrzH21L9oSFdbb3Skrl4dII+hQuPrRlz\n"
  "PveIRPcgLvt3mRS5YA6vrIuT9WfP85qt7QD4ibcDtb5+y93m0ayJty8YcgNCfqrF\n"
  "VdW40deeC4xzfBR6Q6edF4lho9O8Zl/9OTI3n/aS6xBl3qdN9y2hcIU9VTJQPErk\n"
  "jJMPb2ugZCeiXiFCPcZQGwn7oVeQsIbkBpkA7aYjATgIfsr7FVjZu8xkEhxr5aEQ\n"
  "9i/eiKECgYEA/zfrqdCvaYxi2bOa3u1nvRSsUbwsH6OFYye5WQhClqig+MQLN8Nu\n"
  "QZop4NswfwFnsIundcDgWzlT/EnuIA6qcsEW8Y4jRTi1uJqJDyDS3dHFXQHnu6c8\n"
  "oEoHJ5r32r93kbWRaJm+sbBrTOA2bH9bJX5NvfYCHMl0fbiY3UceRqsCgYEAxuMa\n"
  "29qBmjdwL11Udk/rMOLGABIITqL+PtRxsDGT54qG6jkfK6Lb0ZfMo7BfXM65Xr3W\n"
  "e875ErgaZZQaJOd2E1CEEWb0NWe+lMmfwJKz4q/p3bI5FL9ZX6Jr/wpJUDFXv051\n"
  "pxn5PPm9gFzvgkoFYSK5DoG6wmYoh4aGgPD3rNECgYEAhEHMZEH6xO21RC/o7+GD\n"
  "Qt71tZ2YGAU7WHj7egHnz/8u+/tL/OfPuTtUvGuaJBbsTvbwHvuGyH9a4IDHX+F5\n"
  "vuIFK8SGzpZmxXV/1VEjNURBzMLx/bLang3+yy1ph/h01BONePFDev17fWkriuos\n"
  "p69eRjS4P4a+UXBZ90GllOUCgYBVpWrljj0Nah43Z1t974B6ds2JLjrBklMmP1oN\n"
  "4+urY+4hYyPXKLS8l0AapVMLpkIRWHLKsiB0PS+w2ow/pCUmwB9/VvSHIvvhGspe\n"
  "pU4tqk9tltgZ5STZmBolpApaLEV7LpBfu0GnTmyaoGrLkpCqecdzRc5k9JUzd2zo\n"
  "jdw6YQKBgEB5T9gNNRpjjD2D8yJmyVDsCcHKUWO4kPejGhIQJ1hFLBsFv3DFmga8\n"
  "Ku+HluGo0TDABJUv1JBpVDeHBNprX3mHgud4fxKvoeG4eBfrjW5QsqIL34mrzT2b\n"
  "Tf6PmCDYsKOA79p2IYNG0aRiVMyTjb0JaDi/QJY65WR6Q5uoyCae\n"
  "-----END RSA PRIVATE KEY-----";
  len = strlen(rsa);
  fh = fopen("/usr/tmp/cloonix_private_id_rsa", "w");
  if (fh == NULL)
    KOUT("ERROR1 create_cloonix_private_id_rsa");
  wlen = fwrite(rsa, 1, len, fh);
  if (wlen != len)
    KOUT("ERROR2 create_cloonix_private_id_rsa");
  fclose(fh);
  chmod("/usr/tmp/cloonix_private_id_rsa", 0400);
}
/*--------------------------------------------------------------------------*/

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
/*--------------------------------------------------------------------------*/

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
    KOUT("%s", cnf);
  while (fgets(line, 100, fh) != NULL)
    {
    if (sscanf(line, "CLOONIX_NET: %s {", nm) == 1)
      {
      if (!strcmp(nm, name))  
        {
        if (get_ip_pass_port(fh, ip, pass, port))
          KOUT("%s %s", cnf, line);
        result = 1;
        break;
        }
      }
    }
    fclose(fh);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void log_ascii_cmd(char *argv[])
{
  int i;
  if (g_log_cmd != NULL)
    {
    for (i=0;  (argv[i] != NULL); i++)
      fprintf(g_log_cmd, "%s ", argv[i]);
    fprintf(g_log_cmd, "\n");
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
  memset(g_xauthority, 0, MAX_PATH_LEN);
  snprintf(g_xauthority, MAX_PATH_LEN-1,
           "/var/lib/cloonix/%s/Cloonauthority", network_name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void process_ocp(int argc, char **argv, char **new_argv,
                        char *ip, int port, char *passwd)
{
  static char sock[200];
  static char dist[300];
  char ocp_param[200];
  if (argc != 9)
    KOUT("ERROR81 PARAM NUMBER");
  sprintf(sock, "/var/lib/cloonix/%s/nat", argv[2]);
  mkdir(sock, 0777);
  sprintf(ocp_param,
          "%s=%d=%s=nat_%s@user=admin=ip=%s=port=22=cloonix_info_end",
          ip, port, passwd, argv[3], argv[4]);
  new_argv[0] = "/usr/libexec/cloonix/client/cloonix-u2i-scp";
  new_argv[1] = sock;
  new_argv[2] = "-i";
  new_argv[3] = "/usr/tmp/cloonix_private_id_rsa";
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
      KOUT("ERROR81 PARAM NUMBER");
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
      KOUT("ERROR82 PARAM NUMBER");
    }
  else
    KOUT("ERROR8 PARAM NUMBER");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void process_osh(int argc, char **argv, char **new_argv,
                        char *ip, int port, char *passwd)
{
  static char sock[200];
  static char ocp_param[200];
  int i;
  if (argc < 5)
    KOUT("ERROR91 PARAM NUMBER");
  sprintf(sock, "/var/lib/cloonix/%s/nat", argv[2]);
  mkdir(sock, 0777); 
  sprintf(ocp_param,
          "%s=%d=%s=nat_%s@user=admin=ip=%s=port=22=cloonix_info_end",
          ip, port, passwd, argv[3], argv[4]);
  new_argv[0] = "/usr/libexec/cloonix/client/cloonix-u2i-ssh";
  new_argv[1] = sock;
  new_argv[2] = "-i";
  new_argv[3] = "/usr/tmp/cloonix_private_id_rsa";
  new_argv[4] = ocp_param;
  for (i=0; i<20; i++)
    {
    if (argc > 5+i)
      new_argv[5+i] = argv[5+i];
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int initialise_new_argv(int argc, char **argv, char **new_argv,
                               char *ip, int port, char *passwd)
{
  static char ipport[100];
  static char title[120];
  static char sock[200];
  static char param[400];
  int chld_state, i, result = 0;
/*CLOONIX_LSH-----------------------*/
  if (!strcmp("lsh", argv[1]))
    {
    new_argv[0] = "/usr/libexec/cloonix/common/sh";
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
      for (i=0; i<MAX_NARGS; i++)
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
    if (argc < 5)
      KOUT("ERROR5 PARAM NUMBER %d", argc);
    sprintf(ipport, "%s:%d", ip, port);
    new_argv[0] = "/usr/libexec/cloonix/client/cloonix-dropbear-scp";
    new_argv[1] = ipport;
    new_argv[2] = passwd;
    for (i=0; i<MAX_NARGS; i++)
      {
      if (argc > 3+i)
        new_argv[3+i] = argv[3+i];
      }
    }
/*CLOONIX_SSH-----------------------*/
  else if (!strcmp("ssh", argv[1]))
    {
    if ((argc != 4) && (argc != 5))
      KOUT("ERROR5 PARAM NUMBER %d", argc);
    sprintf(ipport, "%s:%d", ip, port);
    new_argv[0] = "/usr/libexec/cloonix/client/cloonix-dropbear-ssh";
    new_argv[1] = ipport;
    new_argv[2] = passwd;
    new_argv[3] = argv[3];
    if (argc == 5)
      new_argv[4] = argv[4];
    }
/*CLOONIX_RSH-----------------------*/
  else if (!strcmp("rsh", argv[1]))
    {
    if (argc != 4)
      KOUT("ERROR5 PARAM NUMBER %d", argc);
    memset(title, 0, 120);
    memset(param, 0, 400);
    snprintf(title, 119, "%s/%s", argv[2], argv[3]);
    snprintf(param, 399, "/usr/libexec/cloonix/server/cloonix-crun "
                         "--log=/var/lib/cloonix/%s/log/debug_crun.log "
                         "--root=/var/lib/cloonix/%s/crun/ exec %s /bin/bash",
                         argv[2], argv[2], argv[3]);
    new_argv[0] = "/usr/libexec/cloonix/client/cloonix-urxvt";
    new_argv[1] = "-T";
    new_argv[2] = title;
    new_argv[3] = "-e";
    new_argv[4] = "/usr/libexec/cloonix/client/cloonix-xwycli";
    new_argv[5] = "/usr/libexec/cloonix/common/etc/cloonix.cfg";
    new_argv[6] = argv[2];
    new_argv[7] = "-crun";
    new_argv[8] = param;
    }
/*CLOONIX_ICE-----------------------*/
  else if (!strcmp("ice", argv[1]))
    {
    if (argc != 5)
      KOUT("ERROR5 PARAM NUMBER %d", argc);
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
      KOUT("ERROR5 PARAM NUMBER %d", argc);
    sprintf(sock, "/var/lib/cloonix/%s/snf/%s_%s", argv[2], argv[3], argv[4]);
    new_argv[0] = "/usr/libexec/cloonix/client/cloonix-xwycli";
    new_argv[1] = "/usr/libexec/cloonix/common/etc/cloonix.cfg";
    new_argv[2] = argv[2];
    new_argv[3] = "-dae";
    new_argv[4] = "/usr/libexec/cloonix/server/cloonix-wireshark";
    new_argv[5] = "-o";
    new_argv[6] = "capture.no_interface_load:TRUE";
    new_argv[7] = "-o";
    new_argv[8] = "gui.ask_unsaved:FALSE";
    new_argv[9] = "-k";
    new_argv[10] = "-i";
    new_argv[11] = sock;
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
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{
  int fd;
  char passwd[80];
  char ip[80];
  char ocp_param[300];
  int  port, pid;
  char *cfg="/usr/libexec/cloonix/common/etc/cloonix.cfg";
  char *new_argv[MAX_NARGS+10];
  uid_t g_my_uid = getuid();
  gid_t g_my_gid = getgid();
  g_log_cmd = NULL;
  memset(new_argv, 0, (MAX_NARGS+10)*sizeof(char *));
  if (argc < 3)
    {
    if ((strcmp("lsh", argv[1])) &&
        (strcmp("cli", argv[1])))
      KOUT("ERROR1 PARAMS");
    }
  else if (!check_net_name(cfg, argv[2], ip, passwd, &port))
    KOUT("ERROR3 PARAMS");
  else
    {
    init_log_cmd(argv[2]);
    }
  if (strcmp("ice", argv[1]))
    {
    assert(unshare(CLONE_NEWNS | CLONE_NEWUSER) == 0);
    become_uid0(g_my_uid, g_my_gid);
    setup_mounts();
    assert(unshare(CLONE_NEWUSER) == 0);
    become_orig(g_my_uid, g_my_gid);
    }
/*--------------------------------------------------------------------------*/
  if (initialise_new_argv(argc, argv, new_argv, ip, port, passwd))
    KOUT("ERROR10 PARAM %s", argv[1]);
/*--------------------------------------------------------------------------*/
  set_env_global_cloonix();
  log_ascii_cmd(new_argv);
/*--------------------------------------------------------------------------*/
  if ((!strcmp("ice", argv[1])) ||
      (!strcmp("wsk", argv[1])) ||
      (!strcmp("rsh", argv[1])))
    {
    pid = fork();
    if (pid < 0)
      KOUT("ERROR10 PARAM %s", argv[1]);
    else if (pid == 0)
      execv(new_argv[0], new_argv);
    }
  else
    {
    if ((!strcmp("osh", argv[1])) ||
        (!strcmp("ocp", argv[1])) ||
        (!strcmp("lsh", argv[1])))
      create_cloonix_private_id_rsa();
    execv(new_argv[0], new_argv);
    }
/*--------------------------------------------------------------------------*/
}
