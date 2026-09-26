/* disabled_protos.c
 * Declarations of routines for reading and writing protocols file that determine
 * enabling and disabling of protocols.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#define WS_LOG_DOMAIN LOG_DOMAIN_EPAN

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <glib.h>

#include <epan/proto.h>
#include <epan/packet.h>

#include "disabled_protos.h"
#include <wsutil/file_util.h>
#include <wsutil/report_message.h>

#define ENABLED_PROTOCOLS_FILE_NAME     "enabled_protos"
#define DISABLED_PROTOCOLS_FILE_NAME    "disabled_protos"
#define HEURISTICS_FILE_NAME            "heuristic_protos"

/*
 * Item in a list of disabled protocols.
 */
typedef struct {
  char *name;		/* protocol name */
} protocol_def;

/*
 * Item in a list of heuristic dissectors and their enabled state.
 */
typedef struct {
  char *name;		/* heuristic short name */
  bool enabled;	/* heuristic enabled */
} heur_protocol_def;

/*
 * List of disabled protocols
 */
static GList *global_disabled_protos;
static GList *disabled_protos;
/*
 * List of enabled protocols (that are disabled by default)
 */
static GList *global_enabled_protos;
static GList *enabled_protos;
/*
 * List of disabled heuristics
 */
static GList *global_disabled_heuristics;
static GList *disabled_heuristics;

static bool unsaved_changes;

#define INIT_BUF_SIZE   128

bool
enabled_protos_unsaved_changes(void)
{
  return unsaved_changes;
}

static void
discard_existing_list (GList **flp)
{
  GList      *fl_ent;
  protocol_def *prot;

  if (*flp != NULL) {
    fl_ent = g_list_first(*flp);
    while (fl_ent != NULL) {
      prot = (protocol_def *) fl_ent->data;
      g_free(prot->name);
      g_free(prot);
      fl_ent = fl_ent->next;
    }
    g_list_free(*flp);
    *flp = NULL;
  }
}

static void
heur_discard_existing_list (GList **flp)
{
  GList      *fl_ent;
  heur_protocol_def *prot;

  if (*flp != NULL) {
    fl_ent = g_list_first(*flp);
    while (fl_ent != NULL) {
      prot = (heur_protocol_def *) fl_ent->data;
      g_free(prot->name);
      g_free(prot);
      fl_ent = fl_ent->next;
    }
    g_list_free(*flp);
    *flp = NULL;
  }
}

/*
 * Enable/Disable protocols as per the stored configuration
 */
static void
set_protos_list(GList *protos_list, GList *global_protos_list, bool enable)
{
  int i;
  GList *fl_ent;
  protocol_def *prot;

  /*
   * Assume no protocols disabled by default wants to be enabled
   */
  if (protos_list == NULL)
    goto skip;

  fl_ent = g_list_first(protos_list);

  while (fl_ent != NULL) {
    prot = (protocol_def *) fl_ent->data;
    i = proto_get_id_by_filter_name(prot->name);
    if (i == -1) {
      /* XXX - complain here? */
    } else {
      if (proto_can_toggle_protocol(i))
        proto_set_decoding(i, enable);
    }

    fl_ent = fl_ent->next;
  }

skip:
  if (global_protos_list == NULL)
    return;

  fl_ent = g_list_first(global_protos_list);

  while (fl_ent != NULL) {
    prot = (protocol_def *) fl_ent->data;
    i = proto_get_id_by_filter_name(prot->name);
    if (i == -1) {
      /* XXX - complain here? */
    } else {
      if (proto_can_toggle_protocol(i)) {
        proto_set_decoding(i, enable);
        proto_set_cant_toggle(i);
      }
    }

    fl_ent = fl_ent->next;
  }
}

/*
 * Write out a list of protocols based on condition
 *
 * On success, "*pref_path_return" is set to NULL.
 * On error, "*pref_path_return" is set to point to the pathname of
 * the file we tried to read - it should be freed by our caller -
 * and "*errno_return" is set to the error.
 */
static void
save_protos_list(char **pref_path_return, int *errno_return, const char* filename,
                const char* header_comment, bool (*protocol_check)(protocol_t  *protocol))
{
}

static int
read_protos_list_file(const char *ff_path, FILE *ff, GList **flp)
{
  protocol_def *prot;
  int         c;
  char       *prot_name;
  int         prot_name_len;
  int         prot_name_index;
  int         line = 1;
  bool        in_comment = false;


  /* Allocate the protocol name buffer. */
  prot_name_len = INIT_BUF_SIZE;
  prot_name = (char *)g_malloc(prot_name_len + 1);

  for (line = 1; ; line++) {
    /* Lines in a protocol file contain the "filter name" of a protocol
       to be enabled or disabled. */

    /* Skip over leading white space, if any. */
    while ((c = ws_getc_unlocked(ff)) != EOF && g_ascii_isspace(c)) {
      if (c == '\n') {
        /* Blank line. */
        continue;
      }
    }

    if (c == EOF) {
      if (ferror(ff))
        goto error;     /* I/O error */
      else
        break;  /* Nothing more to read */
    }
    ungetc(c, ff);      /* Unread the non-white-space character. */

    /* Get the name of the protocol. */
    prot_name_index = 0;
    for (;;) {
      c = ws_getc_unlocked(ff);
      if (c == EOF)
        break;  /* End of file, or I/O error */
      if (g_ascii_isspace(c))
        break;  /* Trailing white space, or end of line. */
      if (c == '#') {
        in_comment = true;
        break;  /* Start of comment, running to end of line. */
      }
      /* Add this character to the protocol name string. */
      if (prot_name_index >= prot_name_len) {
        /* protocol name buffer isn't long enough; double its length. */
        prot_name_len *= 2;
        prot_name = (char *)g_realloc(prot_name, prot_name_len + 1);
      }
      prot_name[prot_name_index] = c;
      prot_name_index++;
    }

    if (g_ascii_isspace(c) && c != '\n') {
      /* Skip over trailing white space. */
      while ((c = ws_getc_unlocked(ff)) != EOF && c != '\n' && g_ascii_isspace(c))
        ;
      if (c != EOF && c != '\n' && c != '#') {
        /* Non-white-space after the protocol name; warn about it,
           in case we come up with a reason to use it. */
        ws_warning("'%s' line %d has extra stuff after the protocol name.",
                  ff_path, line);
      }
    }
    if (c != EOF && c != '\n' && in_comment == true) {
      /* Skip to end of line. */
      while ((c = ws_getc_unlocked(ff)) != EOF && c != '\n')
        ;
    }

    if (c == EOF) {
      if (ferror(ff))
        goto error;     /* I/O error */
      else {
        /* EOF, not error; no newline seen before EOF */
        ws_warning("'%s' line %d doesn't have a newline.", ff_path,
                  line);
      }
      break;    /* nothing more to read */
    }

    if (in_comment) {
      in_comment = false;
      continue;
    }

    /* Null-terminate the protocol name. */
    if (prot_name_index >= prot_name_len) {
      /* protocol name buffer isn't long enough; double its length. */
      prot_name_len *= 2;
      prot_name = (char *)g_realloc(prot_name, prot_name_len + 1);
    }
    prot_name[prot_name_index] = '\0';

    /* Add the new protocol to the list of disabled protocols */
    prot         = g_new(protocol_def, 1);
    prot->name   = g_strdup(prot_name);
    *flp = g_list_append(*flp, prot);
  }
  g_free(prot_name);
  return 0;

error:
  g_free(prot_name);
  return errno;
}

/*
 * Read in global and personal versions of a list of protocols.
 *
 * If we can open and read the global version, *gpath_return is set to
 * NULL.  Otherwise, *gpath_return is set to point to the pathname of
 * the file we tried to read - it should be freed by our caller - and
 * *gopen_errno_return is set to the error if an open failed or
 * *gread_errno_return is set to the error if a read failed.
 *
 * If we can open and read the personal version, *path_return is set to
 * NULL.  Otherwise, *path_return is set to point to the pathname of
 * the file we tried to read - it should be freed by our caller - and
 * *open_errno_return is set to the error if an open failed or
 * *read_errno_return is set to the error if a read failed.
 */
static void
read_protos_list(char **gpath_return, int *gopen_errno_return,
                 int *gread_errno_return,
                 char **path_return, int *open_errno_return,
                 int *read_errno_return,
                 const char* filename,
                 GList **global_protos_list, GList **protos_list)
{
}

/************************************************************************
 * Disabling dissectors
 ************************************************************************/

/*
 * Disable a particular protocol by name
 */
bool
proto_disable_proto_by_name(const char *name)
{
  protocol_t *protocol;
  int proto_id;

  proto_id = proto_get_id_by_filter_name(name);
  if (proto_id >= 0 ) {
    protocol = find_protocol_by_id(proto_id);
    if (proto_is_protocol_enabled(protocol) == true) {
      if (proto_can_toggle_protocol(proto_id) == true) {
        unsaved_changes = true;
        proto_set_decoding(proto_id, false);
      }
    }
    return true;
  }
  else if (!strcmp(name, "ALL")) {
    unsaved_changes = true;
    proto_disable_all();
    return true;
  }
  else {
    return false;
  }
}

static bool disable_proto_list_check(protocol_t  *protocol)
{
    if (proto_is_protocol_enabled(protocol) == false)
      return true;

    return false;
}

/************************************************************************
 * Enabling dissectors (that are disabled by default)
 ************************************************************************/

bool
proto_enable_proto_by_name(const char *name)
{
  protocol_t *protocol;
  int proto_id;

  proto_id = proto_get_id_by_filter_name(name);
  if (proto_id >= 0 ) {
    protocol = find_protocol_by_id(proto_id);
    if ((proto_is_protocol_enabled(protocol) == false)) {
      if (proto_can_toggle_protocol(proto_id) == true) {
        unsaved_changes = true;
        proto_set_decoding(proto_id, true);
      }
    }
    return true;
  }
  else if (!strcmp(name, "ALL")) {
    unsaved_changes = true;
    proto_reenable_all();
    return true;
  }
  else {
    return false;
  }
}

static bool enable_proto_list_check(protocol_t  *protocol)
{
  if ((proto_is_protocol_enabled_by_default(protocol) == false) &&
      (proto_is_protocol_enabled(protocol) == true))
    return true;

  return false;
}

/************************************************************************
 * Heuristic dissectors
 ************************************************************************/


static void
set_disabled_heur_dissector_list(void)
{
  GList *fl_ent;
  heur_protocol_def *heur;
  heur_dtbl_entry_t* h;

  if (disabled_heuristics == NULL)
    goto skip;

  fl_ent = g_list_first(disabled_heuristics);

  while (fl_ent != NULL) {
    heur = (heur_protocol_def *) fl_ent->data;
    h = find_heur_dissector_by_unique_short_name(heur->name);
    if (h != NULL) {
      h->enabled = heur->enabled;
    }

    fl_ent = fl_ent->next;
  }

skip:
  if (global_disabled_heuristics == NULL)
    return;

  fl_ent = g_list_first(global_disabled_heuristics);

  while (fl_ent != NULL) {
    heur = (heur_protocol_def *) fl_ent->data;

    h = find_heur_dissector_by_unique_short_name(heur->name);
    if (h != NULL) {
      h->enabled = heur->enabled;
    }

    fl_ent = fl_ent->next;
  }
}

static int
read_heur_dissector_list_file(const char *ff_path, FILE *ff, GList **flp)
{
  heur_protocol_def *heur;
  int         c;
  char       *heuristic_name;
  int         heuristic_name_len;
  int         name_index;
  bool        parse_enabled;
  bool        enabled;
  int         line = 1;


  /* Allocate the protocol name buffer. */
  heuristic_name_len = INIT_BUF_SIZE;
  heuristic_name = (char *)g_malloc(heuristic_name_len + 1);

  for (line = 1; ; line++) {
    /* Lines in a disabled protocol file contain the "filter name" of
       a protocol to be disabled. */

    /* Skip over leading white space, if any. */
    while ((c = ws_getc_unlocked(ff)) != EOF && g_ascii_isspace(c)) {
      if (c == '\n') {
        /* Blank line. */
        continue;
      }
    }

    if (c == EOF) {
      if (ferror(ff))
        goto error;     /* I/O error */
      else
        break;  /* Nothing more to read */
    }
    ungetc(c, ff);      /* Unread the non-white-space character. */

    /* Get the name of the protocol. */
    name_index = 0;
    enabled = false;
    parse_enabled = false;
    for (;;) {
      c = ws_getc_unlocked(ff);
      if (c == EOF)
        break;  /* End of file, or I/O error */
      if (g_ascii_isspace(c))
        break;  /* Trailing white space, or end of line. */
      if (c == ',') {/* Separator for enable/disable */
        parse_enabled = true;
        continue;
      }
      if (c == '#')
        break;  /* Start of comment, running to end of line. */
      if (parse_enabled) {
          enabled = ((c == '1') ? true : false);
          break;
      }
      /* Add this character to the protocol name string. */
      if (name_index >= heuristic_name_len) {
        /* protocol name buffer isn't long enough; double its length. */
        heuristic_name_len *= 2;
        heuristic_name = (char *)g_realloc(heuristic_name, heuristic_name_len + 1);
      }
      heuristic_name[name_index] = c;
      name_index++;
    }

    if (g_ascii_isspace(c) && c != '\n') {
      /* Skip over trailing white space. */
      while ((c = ws_getc_unlocked(ff)) != EOF && c != '\n' && g_ascii_isspace(c))
        ;
      if (c != EOF && c != '\n' && c != '#') {
        /* Non-white-space after the protocol name; warn about it,
           in case we come up with a reason to use it. */
        ws_warning("'%s' line %d has extra stuff after the protocol name.",
                  ff_path, line);
      }
    }
    if (c != EOF && c != '\n') {
      /* Skip to end of line. */
      while ((c = ws_getc_unlocked(ff)) != EOF && c != '\n')
        ;
    }

    if (c == EOF) {
      if (ferror(ff))
        goto error;     /* I/O error */
      else {
        /* EOF, not error; no newline seen before EOF */
        ws_warning("'%s' line %d doesn't have a newline.", ff_path,
                  line);
      }
      break;    /* nothing more to read */
    }

    /* Null-terminate the protocol name. */
    if (name_index >= heuristic_name_len) {
      /* protocol name buffer isn't long enough; double its length. */
      heuristic_name_len *= 2;
      heuristic_name = (char *)g_realloc(heuristic_name, heuristic_name_len + 1);
    }
    heuristic_name[name_index] = '\0';

    /* Add the new protocol to the list of protocols */
    heur         = g_new(heur_protocol_def, 1);
    heur->name   = g_strdup(heuristic_name);
    heur->enabled = enabled;
    *flp = g_list_append(*flp, heur);
  }
  g_free(heuristic_name);
  return 0;

error:
  g_free(heuristic_name);
  return errno;
}

static void
read_heur_dissector_list(char **gpath_return, int *gopen_errno_return,
			 int *gread_errno_return,
			 char **path_return, int *open_errno_return,
			 int *read_errno_return)
{
}

static int
heur_compare(const void *a, const void *b)
{
  return strcmp(((const heur_dtbl_entry_t *)a)->short_name,
                ((const heur_dtbl_entry_t *)b)->short_name);
}

static void
write_heur_dissector(void *data, void *user_data)
{
  heur_dtbl_entry_t* dtbl_entry = (heur_dtbl_entry_t*)data;
  FILE *ff = (FILE*)user_data;

  /* Write out the heuristic short name and its enabled state */
  fprintf(ff, "%s,%d\n", dtbl_entry->short_name, dtbl_entry->enabled ? 1 : 0);
}

static void
sort_dissector_table_entries(const char *table_name ,
    heur_dtbl_entry_t *dtbl_entry, void *user_data)
{
  GSList **list = (GSList**)user_data;
  *list = g_slist_insert_sorted(*list, dtbl_entry, heur_compare);
}

static void
sort_heur_dissector_tables(const char *table_name, struct heur_dissector_list *list, void *w)
{
  if (list) {
    heur_dissector_table_foreach(table_name, sort_dissector_table_entries, w);
  }
}

static void
save_disabled_heur_dissector_list(char **pref_path_return, int *errno_return)
{
}

static bool
proto_set_heuristic_by_name(const char *name, bool enable)
{
  heur_dtbl_entry_t* heur = find_heur_dissector_by_unique_short_name(name);
  if (heur != NULL) {
      unsaved_changes |= (heur->enabled != enable);
      heur->enabled = enable;
      return true;
  } else {
      return false;
  }
}

bool
proto_enable_heuristic_by_name(const char *name)
{
  return proto_set_heuristic_by_name(name, true);
}

bool
proto_disable_heuristic_by_name(const char *name)
{
  return proto_set_heuristic_by_name(name, false);
}

static void
disabled_protos_free(void *p, void *user_data )
{
  protocol_def* pd = (protocol_def*)p;
  g_free(pd->name);
  g_free(p);
}

/*
 * Read the files that enable and disable protocols and heuristic
 * dissectors.  Report errors through the UI.
 */
void
read_enabled_and_disabled_lists(void)
{
  char *gpath = NULL, *path = NULL;
  int gopen_errno = 0, gread_errno = 0;
  int open_errno = 0, read_errno = 0;

  /*
   * Read the global and personal disabled protocols files.
   */
  read_protos_list(&gpath, &gopen_errno, &gread_errno,
                   &path, &open_errno, &read_errno,
                   DISABLED_PROTOCOLS_FILE_NAME,
                   &global_disabled_protos, &disabled_protos);
  if (gpath != NULL) {
    if (gopen_errno != 0) {
      report_warning("Could not open global disabled protocols file\n\"%s\": %s.",
                     gpath, g_strerror(gopen_errno));
    }
    if (gread_errno != 0) {
      report_warning("I/O error reading global disabled protocols file\n\"%s\": %s.",
                     gpath, g_strerror(gread_errno));
    }
    g_free(gpath);
    gpath = NULL;
  }
  if (path != NULL) {
    if (open_errno != 0) {
      report_warning("Could not open your disabled protocols file\n\"%s\": %s.",
                     path, g_strerror(open_errno));
    }
    if (read_errno != 0) {
      report_warning("I/O error reading your disabled protocols file\n\"%s\": %s.",
                     path, g_strerror(read_errno));
    }
    g_free(path);
    path = NULL;
  }

  /*
   * Read the global and personal enabled protocols files.
   */
  read_protos_list(&gpath, &gopen_errno, &gread_errno,
                   &path, &open_errno, &read_errno,
                   ENABLED_PROTOCOLS_FILE_NAME,
                   &global_enabled_protos, &enabled_protos);
  if (gpath != NULL) {
    if (gopen_errno != 0) {
      report_warning("Could not open global enabled protocols file\n\"%s\": %s.",
                     gpath, g_strerror(gopen_errno));
    }
    if (gread_errno != 0) {
      report_warning("I/O error reading global enabled protocols file\n\"%s\": %s.",
                     gpath, g_strerror(gread_errno));
    }
    g_free(gpath);
    gpath = NULL;
  }
  if (path != NULL) {
    if (open_errno != 0) {
      report_warning("Could not open your enabled protocols file\n\"%s\": %s.",
                     path, g_strerror(open_errno));
    }
    if (read_errno != 0) {
      report_warning("I/O error reading your enabled protocols file\n\"%s\": %s.",
                     path, g_strerror(read_errno));
    }
    g_free(path);
    path = NULL;
  }

  /*
   * Read the global and personal heuristic dissector list files.
   */
  read_heur_dissector_list(&gpath, &gopen_errno, &gread_errno,
                           &path, &open_errno, &read_errno);
  if (gpath != NULL) {
    if (gopen_errno != 0) {
      report_warning("Could not open global heuristic dissectors file\n\"%s\": %s.",
                     gpath, g_strerror(gopen_errno));
    }
    if (gread_errno != 0) {
      report_warning("I/O error reading global heuristic dissectors file\n\"%s\": %s.",
                     gpath, g_strerror(gread_errno));
    }
    g_free(gpath);
    gpath = NULL;
  }
  if (path != NULL) {
    if (open_errno != 0) {
      report_warning("Could not open your heuristic dissectors file\n\"%s\": %s.",
                     path, g_strerror(open_errno));
    }
    if (read_errno != 0) {
      report_warning("I/O error reading your heuristic dissectors file\n\"%s\": %s.",
                     path, g_strerror(read_errno));
    }
    g_free(path);
    path = NULL;
  }

  /*
   * Enable/disable protocols and heuristic dissectors as per the
   * contents of the files we just read.
   */
  set_protos_list(disabled_protos, global_disabled_protos, false);
  set_protos_list(enabled_protos, global_enabled_protos, true);
  set_disabled_heur_dissector_list();
  unsaved_changes = false;
}

/*
 * Write out the lists of enabled and disabled protocols and heuristic
 * dissectors to the corresponding files.  Report errors through the UI.
 */
void
save_enabled_and_disabled_lists(void)
{
  char *pf_dir_path = NULL;
  char *pf_path = NULL;
  int pf_save_errno = 0;
  bool ok = true;


  save_protos_list(&pf_path, &pf_save_errno, DISABLED_PROTOCOLS_FILE_NAME,
                   NULL, disable_proto_list_check);
  if (pf_path != NULL) {
    report_failure("Could not save to your disabled protocols file\n\"%s\": %s.",
                   pf_path, g_strerror(pf_save_errno));
    g_free(pf_path);
    ok = false;
  }

  save_protos_list(&pf_path, &pf_save_errno, ENABLED_PROTOCOLS_FILE_NAME,
                   "#This file is for enabling protocols that are disabled by default",
                   enable_proto_list_check);
  if (pf_path != NULL) {
    report_failure("Could not save to your enabled protocols file\n\"%s\": %s.",
                   pf_path, g_strerror(pf_save_errno));
    g_free(pf_path);
    ok = false;
  }

  save_disabled_heur_dissector_list(&pf_path, &pf_save_errno);
  if (pf_path != NULL) {
    report_failure("Could not save to your disabled heuristic protocol file\n\"%s\": %s.",
                   pf_path, g_strerror(pf_save_errno));
    g_free(pf_path);
    ok = false;
  }

  if (ok)
    unsaved_changes = false;
}

void
cleanup_enabled_and_disabled_lists(void)
{
  g_list_foreach(global_disabled_heuristics, disabled_protos_free, NULL);
  g_list_free(global_disabled_heuristics);
  g_list_foreach(disabled_heuristics, disabled_protos_free, NULL);
  g_list_free(disabled_heuristics);
  g_list_foreach(global_disabled_protos, disabled_protos_free, NULL);
  g_list_free(global_disabled_protos);
  g_list_foreach(disabled_protos, disabled_protos_free, NULL);
  g_list_free(disabled_protos);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local Variables:
 * c-basic-offset: 2
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set shiftwidth=2 tabstop=8 expandtab:
 * :indentSize=2:tabSize=8:noTabs=true:
 */
