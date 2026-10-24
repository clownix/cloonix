/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* The exec pipe wire, read by the linux and windows cxp servers alike. */
/* Wire format, both directions: */
/* 4 bytes big endian length (opcode + payload), 1 opcode byte, payload */
/* client -> guest                      guest -> client */
/* 0x01 EXEC   json {cmd}               0x81 STDOUT raw */
/* 0x02 PTY    json {cmd,cols,rows}     0x82 STDERR raw */
/* 0x03 STDIN  raw                      0x83 EXIT   json {code} */
/* 0x04 EOF                             0x84 ERROR  json {txt} */
/* 0x05 WINCH  json {cols,rows}         0x85 META   json {size,mode} */
/* 0x06 PUT    json {path,name,mode}    0x86 DATA   raw */
/* 0x07 DATA   raw                      0x87 END */
/* 0x08 END */
/* 0x09 GET    json {path} */
/* 0x0A PUTTREE json {path}             0x8B ENTRY  json {name,type, */
/* 0x0B ENTRY  json {name,type,mode,            mode,size,link} */
/* size,link} */
/* 0x0C GETTREE json {path} */
/* 0x0D LIST    json {path} */
/* A tree: PUTTREE, then per member ENTRY followed by its DATA frames, then */
/* END. type is "d", "f" or "l", link holds a symlink target. Names are */
/* relative, "/" separated, refused if absolute or holding "..". GETTREE is */
/* the reverse. LIST: one directory, an ENTRY per bare member name, END, */
/* then EXIT carrying the path the guest settled on. */
#ifndef CXP_WIRE_H
#define CXP_WIRE_H

#define OP_EXEC    0x01
#define OP_PTY     0x02
#define OP_STDIN   0x03
#define OP_EOF     0x04
#define OP_WINCH   0x05
#define OP_PUT     0x06
#define OP_DATA    0x07
#define OP_END     0x08
#define OP_GET     0x09
#define OP_PUTTREE 0x0A
#define OP_ENTRY   0x0B
#define OP_GETTREE 0x0C
#define OP_LIST    0x0D

#define OP_STDOUT  0x81
#define OP_STDERR  0x82
#define OP_EXIT    0x83
#define OP_ERROR   0x84
#define OP_META    0x85
#define OP_GDATA   0x86
#define OP_GEND    0x87
#define OP_GENTRY  0x8B

/* The linux cxp server gives its KERR lines to the agent, which sends them
   to the host: one datagram a line. */
#define UNIX_KERR_SOCK "/cloonixmnt/tmp/unix_cloonix_kerr_sock"

/* Payloads are flat objects of a few keys: strstr parsing is enough. */
void cxp_json_str(const char *body, const char *key, char *out, int outsz);
int  cxp_json_int(const char *body, const char *key, int dflt);

/* Every payload string goes through this: a bare backslash breaks json. */
void cxp_json_escape(const char *in, char *out, int outsz);

/* 0 when the name stays inside the destination: not absolute, no drive
 * letter, no empty component, no "..". */
int cxp_safe_rel_path(const char *name);

#endif
