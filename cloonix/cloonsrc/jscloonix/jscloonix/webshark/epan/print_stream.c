/* print_stream.c
 * Routines for print streams.
 *
 * Gilbert Ramirez <gram@alumni.rice.edu>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <unistd.h>

#include <glib.h>

#include <epan/print_stream.h>

#include <wsutil/file_util.h>

#define TERM_SGR_RESET "\x1B[0m"  /* SGR - reset */
#define TERM_CSI_EL    "\x1B[K"   /* EL - Erase in Line (to end of line) */

typedef enum {
    COLOR_NONE,
    COLOR_24BIT_ESCAPE
} color_type_t;

typedef struct {
    bool         to_file;
    FILE        *fh;
    bool         isatty;
    const char  *to_codeset;
    color_type_t color_type;
} output_text;

static void
set_color_24bit_escape(FILE *fh, const color_t *fg, const color_t *bg)
{
    if (fg) {
        fprintf(fh, "\x1B[38;2;%u;%u;%um",
                (fg->red   >> 8) & 0xff,
                (fg->green >> 8) & 0xff,
                (fg->blue  >> 8) & 0xff);
    }

    if (bg) {
        fprintf(fh, "\x1B[48;2;%u;%u;%um",
                (bg->red   >> 8) & 0xff,
                (bg->green >> 8) & 0xff,
                (bg->blue  >> 8) & 0xff);
    }
}


static void
do_color_eol_24bit_escape(print_stream_t *self)
{
    output_text *output = (output_text *)self->data;
    FILE *fh = output->fh;
    fprintf(fh, "%s\n%s", TERM_CSI_EL, TERM_SGR_RESET);
}

static FILE *
open_print_dest(bool to_file, const char *dest)
{
    FILE *fh;

    if (to_file)
        fh = ws_fopen(dest, "w");
    else
        fh = popen(dest, "w");

    return fh;
}

static bool
close_print_dest(bool to_file, FILE *fh)
{
    if (to_file)
        return (fclose(fh) == 0);
    else
        return (pclose(fh) == 0);
}

bool
print_preamble(print_stream_t *self, char *filename, const char *version_string)
{
    return self->ops->print_preamble ? (self->ops->print_preamble)(self, filename, version_string) : true;
}

bool
print_line(print_stream_t *self, int indent, const char *line)
{
    return (self->ops->print_line)(self, indent, line);
}

bool
print_line_color(print_stream_t *self, int indent, const char *line, const color_t *fg, const color_t *bg)
{
    if (self->ops->print_line_color)
        return (self->ops->print_line_color)(self, indent, line, fg, bg);
    else
        return (self->ops->print_line)(self, indent, line);
}

/* Insert bookmark */
bool
print_bookmark(print_stream_t *self, const char *name, const char *title)
{
    return self->ops->print_bookmark ? (self->ops->print_bookmark)(self, name, title) : true;
}

bool
new_page(print_stream_t *self)
{
    return self->ops->new_page ? (self->ops->new_page)(self) : true;
}

/* Some formats need stuff at the end of the output */
bool
print_finale(print_stream_t *self)
{
    return self->ops->print_finale ? (self->ops->print_finale)(self) : true;
}

bool
destroy_print_stream(print_stream_t *self)
{
    return (self && self->ops && self->ops->destroy) ? (self->ops->destroy)(self) : true;
}

#define MAX_INDENT    160

static bool
print_line_color_text(print_stream_t *self, int indent, const char *line, const color_t *fg, const color_t *bg)
{
    static char spaces[MAX_INDENT];
    size_t ret;
    output_text *output = (output_text *)self->data;
    unsigned int num_spaces;
    bool emit_color = output->isatty && (fg != NULL || bg != NULL);

    /* should be space, if NUL -> initialize */
    if (!spaces[0])
        memset(spaces, ' ', sizeof(spaces));

    if (emit_color) {
        switch (output->color_type) {

        case COLOR_NONE:
            break;

        case COLOR_24BIT_ESCAPE:
            set_color_24bit_escape(output->fh, fg, bg);
            if (ferror(output->fh))
                return false;
            break;
        }
    }

    num_spaces = indent * 4;
    if (num_spaces > MAX_INDENT)
        num_spaces = MAX_INDENT;

    ret = fwrite(spaces, 1, num_spaces, output->fh);
    if (ret == num_spaces) {
        if (output->isatty && output->to_codeset) {
            char *tty_out;
            tty_out = g_convert_with_fallback(line, -1, output->to_codeset, "UTF-8", "?", NULL, NULL, NULL);

            if (tty_out) {
                fputs(tty_out, output->fh);
                g_free(tty_out);
            } else {
                fputs(line, output->fh);
            }
        } else {
            fputs(line, output->fh);
        }


        if (emit_color) {
            switch (output->color_type) {

            case COLOR_NONE:
                putc('\n', output->fh);
                break;

            case COLOR_24BIT_ESCAPE:
                do_color_eol_24bit_escape(self);
                break;
            }
        } else
            putc('\n', output->fh);
    }

    return !ferror(output->fh);
}

static bool
print_line_text(print_stream_t *self, int indent, const char *line)
{
    return print_line_color_text(self, indent, line, NULL, NULL);
}

static bool
new_page_text(print_stream_t *self)
{
    output_text *output = (output_text *)self->data;

    fputs("\f", output->fh);
    return !ferror(output->fh);
}

static bool
destroy_text(print_stream_t *self)
{
    output_text *output = (output_text *)self->data;
    bool         ret;

    switch (output->color_type) {

    case COLOR_NONE:
        break;

    case COLOR_24BIT_ESCAPE:
        /* Reset the color to the default */
        fprintf(output->fh, "%s", TERM_SGR_RESET);
        fflush(output->fh);
        break;
    }

    ret = close_print_dest(output->to_file, output->fh);
    g_free(output);
    g_free(self);
    return ret;
}

static const print_stream_ops_t print_text_ops = {
    NULL,            /* preamble */
    print_line_text,
    print_line_color_text,
    NULL,            /* bookmark */
    new_page_text,
    NULL,            /* finale */
    destroy_text,
};

static print_stream_t *
print_stream_text_alloc(bool to_file, FILE *fh)
{
    print_stream_t *stream;
    output_text    *output;

    output          = (output_text *)g_malloc(sizeof *output);
    output->to_file = to_file;
    output->fh      = fh;

    /*
     * On UN*X, isatty() tests "whether fildes, an open file descriptor,
     * is associated with a terminal device", to quote the Single UNIX
     * Specification, and the documentation for UN*Xes that haven't
     * been tested against the SUS validation suite say similar things.
     * It does *not* just test whether it's associated with a character
     * device that may or may not be a terminal device, so it's what we
     * want on UN*X.
     */
    output->isatty = isatty(ws_fileno(fh));
    if (output->isatty) {
        const char *charset;
        bool is_utf8;

        /* Is there a more reliable way to do this? */
        is_utf8 = g_get_charset(&charset);
        if (!is_utf8) {
            /*
             * The local character set isn't UTF-8, so arrange to
             * map from UTF-8 to that character set before printing
             * on the terminal.
             */
            output->to_codeset = charset;
        } else {
            /*
             * The local character set is UTF-8, so no mapping is
             * necessary.
             */
            output->to_codeset = NULL;
        }

        char *colorterm = getenv("COLORTERM");
        if (colorterm != NULL &&
            (strcmp(colorterm, "truecolor") == 0 || strcmp(colorterm, "24bit") == 0))
            output->color_type = COLOR_24BIT_ESCAPE;
        else
            output->color_type = COLOR_NONE;
    }
    if (!output->isatty) {
        output->to_codeset = NULL;
        output->color_type = COLOR_NONE;
    }

    stream          = g_new(print_stream_t, 1);
    stream->ops     = &print_text_ops;
    stream->data    = output;

    return stream;
}

print_stream_t *
print_stream_text_new(bool to_file, const char *dest)
{
    FILE *fh;

    fh = open_print_dest(to_file, dest);
    if (fh == NULL)
        return NULL;

    return print_stream_text_alloc(to_file, fh);
}

print_stream_t *
print_stream_text_stdio_new(FILE *fh)
{
    return print_stream_text_alloc(true, fh);
}

typedef struct {
    bool      to_file;
    FILE     *fh;
} output_ps;

#define MAX_PS_LINE_LENGTH 256

static
void ps_clean_string(char *out, const char *in, int outbuf_size)
{
    int  rd, wr;
    char c;

    if (in == NULL) {
        out[0] = '\0';
        return;
    }

    for (rd = 0, wr = 0 ; wr < outbuf_size; rd++, wr++ ) {
        c = in[rd];
        switch (c) {
        case '(':
        case ')':
        case '\\':
            out[wr] = '\\';
            out[++wr] = c;
            break;

        default:
            out[wr] = c;
            break;
        }

        if (c == 0) {
            break;
        }
    }
}

static bool
print_preamble_ps(print_stream_t *self, char *filename, const char *version_string)
{
    output_ps *output = (output_ps *)self->data;
    char       psbuffer[MAX_PS_LINE_LENGTH]; /* static sized buffer! */

    fputs("%% the page title\n", output->fh);
    ps_clean_string(psbuffer, filename, MAX_PS_LINE_LENGTH);
    fprintf(output->fh, "/ws_pagetitle (%s - Wireshark %s) def\n", psbuffer, version_string);
    fputs("\n", output->fh);
    return !ferror(output->fh);
}

static bool
print_line_ps(print_stream_t *self, int indent, const char *line)
{
    output_ps *output = (output_ps *)self->data;
    char       psbuffer[MAX_PS_LINE_LENGTH]; /* static sized buffer! */

    ps_clean_string(psbuffer, line, MAX_PS_LINE_LENGTH);
    fprintf(output->fh, "%d (%s) putline\n", indent, psbuffer);
    return !ferror(output->fh);
}

static bool
print_bookmark_ps(print_stream_t *self, const char *name, const char *title)
{
    output_ps *output = (output_ps *)self->data;
    char       psbuffer[MAX_PS_LINE_LENGTH]; /* static sized buffer! */

    /*
     * See the Adobe "pdfmark reference":
     *
     *  http://partners.adobe.com/asn/acrobat/docs/pdfmark.pdf
     *
     * The pdfmark stuff tells code that turns PostScript into PDF
     * things that it should do.
     *
     * The /OUT stuff creates a bookmark that goes to the
     * destination with "name" as the name and "title" as the title.
     *
     * The "/DEST" creates the destination.
     */
    ps_clean_string(psbuffer, title, MAX_PS_LINE_LENGTH);
    fprintf(output->fh, "[/Dest /%s /Title (%s)   /OUT pdfmark\n", name,
          psbuffer);
    fputs("[/View [/XYZ -4 currentpoint matrix currentmatrix matrix defaultmatrix\n",
          output->fh);
    fputs("matrix invertmatrix matrix concatmatrix transform exch pop 20 add null]\n",
          output->fh);
    fprintf(output->fh, "/Dest /%s /DEST pdfmark\n", name);
    return !ferror(output->fh);
}

static bool
new_page_ps(print_stream_t *self)
{
    output_ps *output = (output_ps *)self->data;

    fputs("formfeed\n", output->fh);
    return !ferror(output->fh);
}

static bool
print_finale_ps(print_stream_t *self)
{
    output_ps *output = (output_ps *)self->data;

    return !ferror(output->fh);
}

static bool
destroy_ps(print_stream_t *self)
{
    output_ps *output = (output_ps *)self->data;
    bool       ret;

    ret = close_print_dest(output->to_file, output->fh);
    g_free(output);
    g_free(self);
    return ret;
}

static const print_stream_ops_t print_ps_ops = {
    print_preamble_ps,
    print_line_ps,
    NULL, /* print_line_color */
    print_bookmark_ps,
    new_page_ps,
    print_finale_ps,
    destroy_ps,
};

static print_stream_t *
print_stream_ps_alloc(bool to_file, FILE *fh)
{
    print_stream_t *stream;
    output_ps      *output;

    output          = (output_ps *)g_malloc(sizeof *output);
    output->to_file = to_file;
    output->fh      = fh;

    stream          = g_new(print_stream_t, 1);
    stream->ops     = &print_ps_ops;
    stream->data    = output;

    return stream;
}

print_stream_t *
print_stream_ps_new(bool to_file, const char *dest)
{
    FILE *fh;

    fh = open_print_dest(to_file, dest);
    if (fh == NULL)
        return NULL;

    return print_stream_ps_alloc(to_file, fh);
}

print_stream_t *
print_stream_ps_stdio_new(FILE *fh)
{
    return print_stream_ps_alloc(true, fh);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
