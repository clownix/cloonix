/* file_wrappers.c
 *
 * Wiretap Library
 * Copyright (c) 1998 by Gilbert Ramirez <gram@alumni.rice.edu>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* file_access interface based heavily on zlib gzread.c and gzlib.c from zlib
 * Copyright (C) 1995-2010 Jean-loup Gailly and Mark Adler
 * under licence:
 *
 * SPDX-License-Identifier: Zlib
 *
 */

#include "config.h"

#define WS_LOG_DOMAIN LOG_DOMAIN_WIRETAP

#include "file_wrappers.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include "wtap-int.h"

#include <wsutil/file_util.h>

static struct compression_type {
    wtap_compression_type  type;
    const char            *extension;
    const char            *description;
    const char            *name;
    const bool            can_write_compressed;
} compression_types[] = {
    { WTAP_UNCOMPRESSED, NULL, NULL, "none", true },
    { WTAP_UNKNOWN_COMPRESSION, NULL, NULL, NULL, false },
};

static wtap_compression_type file_get_compression_type(FILE_T stream);

wtap_compression_type
wtap_name_to_compression_type(const char *name)
{
    for (struct compression_type *p = compression_types;
	    p->type != WTAP_UNKNOWN_COMPRESSION; p++) {
		if (!g_strcmp0(name, p->name))
			return p->type;
	}
    return WTAP_UNKNOWN_COMPRESSION;
}

wtap_compression_type
wtap_extension_to_compression_type(const char *ext)
{
    for (struct compression_type *p = compression_types;
	    p->type != WTAP_UNKNOWN_COMPRESSION; p++) {
		if (!g_strcmp0(ext, p->extension))
			return p->type;
	}
    return WTAP_UNKNOWN_COMPRESSION;
}

bool
wtap_can_write_compression_type(wtap_compression_type compression_type)
{
    for (struct compression_type *p = compression_types; p->type != WTAP_UNKNOWN_COMPRESSION; p++) {
		if (compression_type == p->type)
			return p->can_write_compressed;
	}

    return false;
}

wtap_compression_type
wtap_get_compression_type(wtap *wth)
{
	return file_get_compression_type((wth->fh == NULL) ? wth->random_fh : wth->fh);
}

const char *
wtap_compression_type_description(wtap_compression_type compression_type)
{
	for (struct compression_type *p = compression_types;
	    p->type != WTAP_UNCOMPRESSED; p++) {
		if (p->type == compression_type)
			return p->description;
	}
	return NULL;
}

const char *
wtap_compression_type_extension(wtap_compression_type compression_type)
{
	for (struct compression_type *p = compression_types;
	    p->type != WTAP_UNCOMPRESSED; p++) {
		if (p->type == compression_type)
			return p->extension;
	}
	return NULL;
}

GSList *
wtap_get_all_compression_type_extensions_list(void)
{
	GSList *extensions;

	extensions = NULL;	/* empty list, to start with */

	for (struct compression_type *p = compression_types;
	    p->type != WTAP_UNCOMPRESSED; p++)
		extensions = g_slist_prepend(extensions, (void *)p->extension);

	return extensions;
}

GSList *
wtap_get_all_output_compression_type_names_list(void)
{
	GSList *names;

	names = NULL;	/* empty list, to start with */

	for (struct compression_type *p = compression_types;
	    p->type != WTAP_UNCOMPRESSED; p++) {
            if (p->can_write_compressed)
                names = g_slist_prepend(names, (void *)p->name);
        }

	return names;
}

/* #define GZBUFSIZE 8192 */
#define GZBUFSIZE 4096
#define LZ4BUFSIZE 4194304 // 4MiB, maximum block size

/* values for wtap_reader compression */
typedef enum {
    UNKNOWN,       /* unknown - look for a compression header */
    UNCOMPRESSED,  /* uncompressed - copy input directly */
    ZLIB,          /* decompress a zlib stream */
    GZIP_AFTER_HEADER,
    ZSTD,
    LZ4,
} compression_t;

/*
 * We limit the size of our input and output buffers to 2^30 bytes,
 * because:
 *
 *    1) on Windows with MSVC, the return value of _read() is int,
 *       so the biggest read you can do is INT_MAX, and the biggest
 *       power of 2 below that is 2^30;
 *
 *    2) the "avail_in" and "avail_out" values in a z_stream structure
 *       in zlib are uInts, and those are unsigned ints, and that
 *       imposes a limit on the buffer size when we're reading a
 *       gzipped file.
 *
 * Thus, we use unsigned for the buffer sizes, offsets, amount available
 * from the buffer, etc.
 *
 * If we want an even bigger buffer for uncompressed data, or for
 * some other form of compression, then the unsigned-sized values should
 * be in structure values used only for reading gzipped files, and
 * other values should be used for uncompressed data or data
 * compressed using other algorithms (e.g., in a union).
 */
#define MAX_READ_BUF_SIZE	(1U << 30)

struct wtap_reader_buf {
    uint8_t *buf;  /* buffer */
    uint8_t *next; /* next byte to deliver from buffer */
    unsigned avail;  /* number of bytes available to deliver at next */
};

struct wtap_reader {
    int fd;                     /* file descriptor */
    int64_t raw_pos;            /* current position in file (just to not call lseek()) */
    int64_t pos;                /* current position in uncompressed data */
    unsigned size;              /* buffer size */

    struct wtap_reader_buf in;  /* input buffer, containing compressed data */
    struct wtap_reader_buf out; /* output buffer, containing uncompressed data */

    bool eof;                   /* true if end of input file reached */
    int64_t start;              /* where the gzip data started, for rewinding */
    int64_t raw;                /* where the raw data started, for seeking */
    compression_t compression;  /* type of compression, if any */
    compression_t last_compression; /* last known compression type */
    bool is_compressed;         /* false if completely uncompressed, true otherwise */

    /* seek request */
    int64_t skip;               /* amount to skip (already rewound if backwards) */
    bool seek_pending;          /* true if seek request pending */

    /* error information */
    int err;                    /* error code */
    const char *err_info;       /* additional error information string for some errors */
    GPtrArray *fast_seek;
    void *fast_seek_cur;
};

/* Current read offset within a buffer. */
static unsigned
offset_in_buffer(struct wtap_reader_buf *buf)
{
    /* buf->next points to the next byte to read, and buf->buf points
       to the first byte in the buffer, so the difference between them
       is the offset.

       This will fit in an unsigned int, because it can't be bigger
       than the size of the buffer, which is an unsigned int. */
    return (unsigned)(buf->next - buf->buf);
}

/* Number of bytes of data that are in a buffer. */
static unsigned
bytes_in_buffer(struct wtap_reader_buf *buf)
{
    /* buf->next + buf->avail points just past the last byte of data in
       the buffer.
       Thus, (buf->next + buf->avail) - buf->buf is the number of bytes
       of data in the buffer.

       This will fit in an unsigned, because it can't be bigger
       than the size of the buffer, which is a unsigned. */
    return (unsigned)((buf->next + buf->avail) - buf->buf);
}

/* Reset a buffer, discarding all data in the buffer, so we read into
   it starting at the beginning. */
static void
buf_reset(struct wtap_reader_buf *buf)
{
    buf->next = buf->buf;
    buf->avail = 0;
}

static int
buf_read(FILE_T state, struct wtap_reader_buf *buf)
{
    unsigned space_left, to_read;
    unsigned char *read_ptr;
    ssize_t ret;

    /* How much space is left at the end of the buffer?
       XXX - the output buffer actually has state->size * 2 bytes. */
    space_left = state->size - bytes_in_buffer(buf);
    if (space_left == 0) {
        /* There's no space left, so we start fresh at the beginning
           of the buffer. */
        buf_reset(buf);

        read_ptr = buf->buf;
        to_read = state->size;
    } else {
        /* There's some space left; try to read as much data as we
           can into that space.  We may get less than that if we're
           reading from a pipe or if we're near the end of the file. */
        read_ptr = buf->next + buf->avail;
        to_read = space_left;
    }

    ret = ws_read(state->fd, read_ptr, to_read);
    if (ret < 0) {
        state->err = errno;
        state->err_info = NULL;
        return -1;
    }
    if (ret == 0)
        state->eof = true;
    state->raw_pos += ret;
    buf->avail += (unsigned)ret;
    return 0;
}

static int /* gz_avail */
fill_in_buffer(FILE_T state)
{
    if (state->err != 0)
        return -1;
    if (!state->eof) {
        if (buf_read(state, &state->in) < 0)
            return -1;
    }
    return 0;
}

#define ZLIB_WINSIZE 32768
#define  LZ4_WINSIZE 65536

struct fast_seek_point {
    int64_t out;         /* corresponding offset in uncompressed data */
    int64_t in;          /* offset in input file of first full byte */

    compression_t compression;
    union {
        struct {
#ifdef HAVE_INFLATEPRIME
            int bits;   /* number of bits (1-7) from byte at in - 1, or 0 */
#endif /* HAVE_INFLATEPRIME */
            unsigned char window[ZLIB_WINSIZE]; /* preceding 32K of uncompressed data */

            /* be gentle with Z_STREAM_END, 8 bytes more... Another solution would be to comment checks out */
            uint32_t adler;
            uint32_t total_out;
        } zlib;
#ifdef USE_LZ4
        struct {
            LZ4F_frameInfo_t lz4_info;
            unsigned char lz4_hdr[LZ4F_HEADER_SIZE_MAX];
#if 0
            unsigned char window[LZ4_WINSIZE]; /* preceding 64K of uncompressed data */
#endif
        } lz4;
#endif
    } data;
};

struct zlib_cur_seek_point {
    unsigned char window[ZLIB_WINSIZE]; /* preceding 32K of uncompressed data */
    unsigned int pos;
    unsigned int have;
};

#define SPAN INT64_C(1048576)
static struct fast_seek_point *
fast_seek_find(FILE_T file, int64_t pos)
{
    struct fast_seek_point *smallest = NULL;
    struct fast_seek_point *item;
    unsigned low, i, max;

    if (!file->fast_seek)
        return NULL;

    for (low = 0, max = file->fast_seek->len; low < max; ) {
        i = (low + max) / 2;
        item = (struct fast_seek_point *)file->fast_seek->pdata[i];

        if (pos < item->out)
            max = i;
        else if (pos > item->out) {
            smallest = item;
            low = i + 1;
        } else {
            return item;
        }
    }
    return smallest;
}

static void
fast_seek_header(FILE_T file, int64_t in_pos, int64_t out_pos,
                 compression_t compression)
{
    struct fast_seek_point *item = NULL;

    if (!file->fast_seek) {
        return;
    }

    if (file->fast_seek->len != 0)
        item = (struct fast_seek_point *)file->fast_seek->pdata[file->fast_seek->len - 1];

    /* fast_seek_header always adds a fast seek point, even if less than
     * SPAN from the last one. That is because it used for new streams
     * (including concatenated streams) where the compression type
     * or, for LZ4, compression options, may change.
     */
    if (!item || item->out < out_pos) {
        struct fast_seek_point *val = g_new(struct fast_seek_point,1);
        val->in = in_pos;
        val->out = out_pos;
        val->compression = compression;

#ifdef USE_LZ4
        if (compression == LZ4) {
            val->data.lz4.lz4_info = file->lz4_info;
            memcpy(val->data.lz4.lz4_hdr, file->lz4_hdr, LZ4F_HEADER_SIZE_MAX);
        }
#endif /* USE_LZ4 */
        g_ptr_array_add(file->fast_seek, val);
    }
}

static void
fast_seek_reset(FILE_T state)
{
    switch (state->compression) {

    case UNKNOWN:
        break;

    case UNCOMPRESSED:
        break;

    case ZLIB:
        ws_assert_not_reached();
        break;

    case GZIP_AFTER_HEADER:
        break;

    case ZSTD:
        ws_assert_not_reached();
        break;

    case LZ4:
        ws_assert_not_reached();
        break;


    default:
        ws_assert_not_reached();
        break;
    }
}

static bool
uncompressed_fill_out_buffer(FILE_T state)
{
    if (buf_read(state, &state->out) < 0)
        return false;
    return true;
}

/* Get next byte from input, or -1 if end or error.
 *
 * Note:
 *
 *      1) errors from buf_read(), and thus from fill_in_buffer(), are
 *      "sticky", and fill_in_buffer() won't do any reading if there's
 *      an error;
 *
 *      2) GZ_GETC() returns -1 on an EOF;
 *
 * so it's safe to make multiple GZ_GETC() calls and only check the
 * last one for an error. */
#define GZ_GETC() ((state->in.avail == 0 && fill_in_buffer(state) == -1) ? -1 : \
                   (state->in.avail == 0 ? -1 :                         \
                    (state->in.avail--, *(state->in.next)++)))


/*
 * Check for a gzip header.
 *
 * Based on the gzip-specific stuff gz_head() from zlib does.
 */
static int
check_for_zlib_compression(FILE_T state)
{
    /*
     * Look for the gzip header.  The first two bytes are 31 and 139,
     * and if we find it, return success if we support gzip and an
     * error if we don't.
     */
    if (state->in.next[0] == 31) {
        state->in.avail--;
        state->in.next++;

        /* Make sure the byte after the first byte is present */
        if (state->in.avail == 0 && fill_in_buffer(state) == -1) {
            /* Read error. */
            return -1;
        }
        if (state->in.avail != 0) {
            if (state->in.next[0] == 139) {
                /*
                 * We have what looks like the ID1 and ID2 bytes of a gzip
                 * header.
                 * Continue processing the file.
                 *
                 * XXX - some capture file formats (I'M LOOKING AT YOU,
                 * ENDACE!) can have 31 in the first byte of the file
                 * and 139 in the second byte of the file.  For now, in
                 * those cases, you lose.
                 */
                state->err = WTAP_ERR_DECOMPRESSION_NOT_SUPPORTED;
                state->err_info = "reading gzip-compressed files isn't supported";
                return -1;
            }

            state->in.avail++;
            state->in.next--;
        }
    }
    return 0;
}



/*
 * Check for a Zstandard header.
 */
static int
check_for_zstd_compression(FILE_T state)
{
    if (state->in.avail >= 4
        && state->in.next[0] == 0x28 && state->in.next[1] == 0xb5
        && state->in.next[2] == 0x2f && state->in.next[3] == 0xfd) {
        state->err = WTAP_ERR_DECOMPRESSION_NOT_SUPPORTED;
        state->err_info = "reading zstd-compressed files isn't supported";
        return -1;
    }
    return 0;
}

/*
 * lz4 compression.
 *
 * https://github.com/lz4/lz4/blob/dev/doc/lz4_Frame_format.md
 */
#ifdef USE_LZ4
static void
lz4_fast_seek_add(FILE_T file, struct zlib_cur_seek_point *point , int64_t in_pos, int64_t out_pos)
{
    if (!file->fast_seek) {
        return;
    }

    struct fast_seek_point *item = NULL;

    if (file->fast_seek->len != 0)
        item = (struct fast_seek_point *)file->fast_seek->pdata[file->fast_seek->len - 1];

    /* As of Glib 2.68 GTree has g_tree_upper_bound, or we could use a
     * wmem_tree. However, since our initial read is usually sequential
     * only adding seek points at the end of the ptr array is fast and fine.
     */

    /* don't bother adding jump points between very small blocks (min SPAN) */
    if (!item || item->out + SPAN < out_pos) {
        struct fast_seek_point *val = g_new(struct fast_seek_point,1);
        val->in = in_pos;
        val->out = out_pos;
        val->compression = LZ4;
        val->data.lz4.lz4_info = file->lz4_info;
        memcpy(val->data.lz4.lz4_hdr, file->lz4_hdr, LZ4F_HEADER_SIZE_MAX);
        g_ptr_array_add(file->fast_seek, val);
    }
}

static bool
lz4_fill_out_buffer(FILE_T state)
{
    ws_assert(state->out.avail == 0);

    if (state->in.avail == 0 && fill_in_buffer(state) == -1)
        return false;

    /*
     * We should be at the start of a block. First, determine the size of
     * the block. We tell LZ4F_decompress that there's no room to put
     * the decompressed block; this will make it read the block size
     * header and stop, returning the size of the block (plus next
     * header) as hint of how much input to hand next.
     */

    size_t outBufSize = 0; // Zero so we don't actually consume the block
    size_t inBufSize;

    size_t compressedSize = 0;

    do {
        /* get more input for decompress() */
        if (state->in.avail == 0 && fill_in_buffer(state) == -1)
            break;
        if (state->eof) {
            state->err = WTAP_ERR_SHORT_READ;
            state->err_info = NULL;
            break;
        }

        inBufSize = state->in.avail;
        compressedSize = LZ4F_decompress(state->lz4_dctx, state->out.buf, &outBufSize, state->in.next, &inBufSize, NULL);

        if (LZ4F_isError(compressedSize)) {
            state->err = WTAP_ERR_DECOMPRESS;
            state->err_info = LZ4F_getErrorName(compressedSize);
            return false;
        }

        state->in.next  += (unsigned)inBufSize;
        state->in.avail -= (unsigned)inBufSize;

        if (compressedSize == 0) {
            /* End of Frame */
            state->last_compression = state->compression;
            state->compression = UNKNOWN;
            return true;
        }

        ws_assert(outBufSize == 0);

    } while (compressedSize < LZ4F_BLOCK_HEADER_SIZE);

    /*
     * We don't want to add a fast seek point for the end of frame,
     * especially if there's another frame or other stream after it,
     * which would have the same out position. So add it after the
     * reading the block size - but point to where the block size
     * is so that we'll fast seek to the block size again.
     */
    if (state->lz4_info.blockMode == LZ4F_blockIndependent) {
        /*
         * XXX - If state->lz4_info.blockMode == LZ4F_blockLinked, it doesn't
         * seem like the LZ4 Frame API can handle this, we would need to use
         * the low level Block API and pass the last 64KiB window of data to
         * LZ4_setStreamDecode and use LZ4_decompress_safe_continue (similar
         * to gzip). So for now we can't do fast seek with it (we do add one
         * header at the frame beginning so that concatenated frames and other
         * decompression streams work.)
         */
        lz4_fast_seek_add(state, NULL, state->raw_pos - state->in.avail - LZ4F_BLOCK_HEADER_SIZE, state->pos);
    }

    // Now actually read the entire next block - but not the next header
    compressedSize -= LZ4F_BLOCK_HEADER_SIZE;
    state->out.next = state->out.buf;

    if (compressedSize > state->size) {
        /*
         * What is this? Either bogus, or some new variant of LZ4 Frames with
         * a larger block size we don't support. We could have a buffer
         * overrun if we try to process it.
         *
         * TODO - We could realloc here.
         */
        state->err = WTAP_ERR_DECOMPRESSION_NOT_SUPPORTED;
        state->err_info = "lz4 compressed block size too large";
        return false;
    }

    size_t ret;
    do {
        /* get more input for decompress() */
        if (state->in.avail == 0 && fill_in_buffer(state) == -1)
            break;
        if (state->eof) {
            state->err = WTAP_ERR_SHORT_READ;
            state->err_info = NULL;
            break;
        }

        outBufSize = (state->size << 1) - offset_in_buffer(&state->out);
        inBufSize = MIN(state->in.avail, compressedSize);
        ret = LZ4F_decompress(state->lz4_dctx, state->out.next, &outBufSize, state->in.next, &inBufSize, NULL);

        if (LZ4F_isError(ret)) {
            state->err = WTAP_ERR_DECOMPRESS;
            state->err_info = LZ4F_getErrorName(ret);
            return false;
        }
        state->in.next  += (unsigned)inBufSize;
        state->in.avail -= (unsigned)inBufSize;
        compressedSize -= inBufSize;

        state->out.next += (unsigned)outBufSize;
        state->out.avail += (unsigned)outBufSize;
    } while (compressedSize != 0);

    state->out.next  = state->out.buf;

#if 0
    /* This is an alternative implementation using the lower-level
     * LZ4 Block API. Doing something like this might be necessary
     * to handle linked blocks, because the Frame API doesn't have
     * a method to reset the dictionary / window.
     */
    int outBufSize = state->size << 1;
    uint32_t compressedSize;
    if (gz_next4(state, &compressedSize) == -1) {
        return false;
    }
    if (compressedSize == 0) {
        /* EndMark */
        if (state->lz4_info.contentChecksumFlag) {
            uint32_t xxHash;
            if (gz_next4(state, &xxHash) == -1) {
                return false;
            }
            /* XXX - check hash? */
        }
        state->last_compression = state->compression;
        state->compression = UNKNOWN;
        return true;
    }
    bool uncompressed = compressedSize >> 31;
    compressedSize &= 0x7FFFFFFF;
    if (compressedSize > state->size) {
        // TODO - we could realloc here
        state->err = WTAP_ERR_DECOMPRESSION_NOT_SUPPORTED;
        state->err_info = "lz4 compressed block size too large";
        return false;
    }

    /*
     * We have to read an entire block as we're using the low-level
     * Block API instead of the LZ4 Frame API.
     */
    if (compressedSize > (unsigned)state->in.avail) {
        memmove(state->in.buf, state->in.next, state->in.avail);
        state->in.next = state->in.buf;
        while ((unsigned)state->in.avail < compressedSize) {
            if (state->eof) {
                state->err = WTAP_ERR_SHORT_READ;
                state->err_info = NULL;
                return false;
            }
            if (fill_in_buffer(state) == -1) {
                return false;
            }
        }
    }

    int decompressedSize;
    if (uncompressed) {
        memcpy(state->out.buf, state->in.buf, compressedSize);
        decompressedSize = compressedSize;
    } else {
        decompressedSize = LZ4_decompress_safe(state->in.next, state->out.buf, compressedSize, outBufSize);
        //const size_t ret = LZ4F_decompress(state->lz4_dctx, state->out.buf, &outBufSize, state->in.next, &inBufSize, NULL);
        if (LZ4F_isError(decompressedSize)) {
            state->err = WTAP_ERR_DECOMPRESS;
            state->err_info = LZ4F_getErrorName(decompressedSize);
            return false;
        }
    }

    /*
     * We assume LZ4F_decompress() will not set inBufSize to a
     * value > state->in.avail.
     */
    state->in.next  += compressedSize;
    state->in.avail -= compressedSize;

    state->out.next  = state->out.buf;
    state->out.avail = (unsigned)decompressedSize;

    if (state->lz4_info.blockChecksumFlag == LZ4F_blockChecksumEnabled) {
        uint32_t xxHash;
        if (gz_next4(state, &xxHash) == -1) {
            return false;
        }
        /* XXX - check hash? */
    }
#endif
    return true;
}
#endif /* USE_LZ4 */

/*
 * Check for an lz4 header.
 */
static int
check_for_lz4_compression(FILE_T state)
{
    /*
     * Look for the lz4 header, and, if we find it, return success
     * if we support lz4 and an error if we don't.
     */
    if (state->in.avail >= 4
        && state->in.next[0] == 0x04 && state->in.next[1] == 0x22
        && state->in.next[2] == 0x4d && state->in.next[3] == 0x18) {
#ifdef USE_LZ4
#if LZ4_VERSION_NUMBER >= 10800
        LZ4F_resetDecompressionContext(state->lz4_dctx);
#else /* LZ4_VERSION_NUMBER >= 10800 */
        LZ4F_freeDecompressionContext(state->lz4_dctx);
        const LZ4F_errorCode_t ret = LZ4F_createDecompressionContext(&state->lz4_dctx, LZ4F_VERSION);
        if (LZ4F_isError(ret)) {
            state->err = WTAP_ERR_INTERNAL;
            state->err_info = LZ4F_getErrorName(ret);
            return -1;
        }
#endif /* LZ4_VERSION_NUMBER >= 10800 */
        size_t headerSize = LZ4F_HEADER_SIZE_MAX;
#if LZ4_VERSION_NUMBER >= 10903
        /*
         * In 1.9.3+ we can handle a silly edge case of a tiny valid
         * frame at the end of a file that is smaller than the maximum
         * header size. (lz4frame.h added the function in 1.9.0, but
         * only for the static library; it wasn't exported until 1.9.3)
         */
        while (state->in.avail < LZ4F_MIN_SIZE_TO_KNOW_HEADER_LENGTH) {
            if (fill_in_buffer(state) == -1) {
                return -1;
            }
            if (state->eof) {
                state->err = WTAP_ERR_SHORT_READ;
                state->err_info = NULL;
                return 0;
            }
        }
        headerSize = LZ4F_headerSize(state->in.next, state->in.avail);
        if (LZ4F_isError(headerSize)) {
            state->err = WTAP_ERR_DECOMPRESS;
            state->err_info = LZ4F_getErrorName(headerSize);
            return -1;
        }
#endif /* LZ4_VERSION_NUMBER >= 10903 */
        while (state->in.avail < headerSize) {
            if (fill_in_buffer(state) == -1) {
                return -1;
            }
            if (state->eof) {
                state->err = WTAP_ERR_SHORT_READ;
                state->err_info = NULL;
                return 0;
            }
        }
        size_t inBufSize = state->in.avail;
        memcpy(state->lz4_hdr, state->in.next, headerSize);
        const LZ4F_errorCode_t err = LZ4F_getFrameInfo(state->lz4_dctx, &state->lz4_info, state->in.next, &inBufSize);
        if (LZ4F_isError(err)) {
            state->err = WTAP_ERR_DECOMPRESS;
            state->err_info = LZ4F_getErrorName(err);
            return -1;
        }

        /*
         * XXX - We could check state->lz4_info.blockSizeID here, and
         * only realloc the buffers to a larger value if the max
         * block size is bigger than state->size. Also we could fail
         * on unknown values?
         */
        state->in.avail -= (unsigned)inBufSize;
        state->in.next += (unsigned)inBufSize;

        fast_seek_header(state, state->raw_pos - state->in.avail, state->pos, LZ4);
        state->compression = LZ4;
        state->is_compressed = true;
        return 1;
#else /* USE_LZ4 */
        state->err = WTAP_ERR_DECOMPRESSION_NOT_SUPPORTED;
        state->err_info = "reading lz4-compressed files isn't supported";
        return -1;
#endif /* USE_LZ4 */
    }
    return 0;
}

typedef int (*compression_type_test)(FILE_T);

static compression_type_test const compression_type_tests[] = {
    check_for_zlib_compression,
    check_for_zstd_compression,
    check_for_lz4_compression,
};

/*
 * Used when we haven't yet determined whether we have a compressed file
 * and, if we do, what sort of compressed file it is.
 *
 * Based on the non-gzip-specific stuff that gz_head() from zlib does.
 */
static int
check_for_compression(FILE_T state)
{
    /*
     * If this isn't the first frame / compressed stream, ensure that
     * we're starting at the beginning of the buffer. This shouldn't
     * get called much.
     *
     * This is to avoid edge cases where a previous frame finished but
     * state->in.next is close to the end of the buffer so there isn't
     * much room to put the start of the next frame.
     * This also lets us put back bytes if things go wrong.
     */
    if (state->in.next != state->in.buf) {
        memmove(state->in.buf, state->in.next, state->in.avail);
        state->in.next = state->in.buf;
    }

    /* get some data in the input buffer */
    if (state->in.avail == 0) {
        if (fill_in_buffer(state) == -1)
            return -1;
        if (state->in.avail == 0)
            return 0;
    }

    /*
     * Check for the compression types we support.
     */
    for (size_t i = 0; i < G_N_ELEMENTS(compression_type_tests); i++) {
        int ret;

        ret = compression_type_tests[i](state);
        if (ret == -1)
            return -1;    /* error */
        if (ret == 1)
            return 0;     /* found it */
    }

    /*
     * Some other compressed file formats we might want to support:
     *
     *   XZ format:
     *     https://tukaani.org/xz/
     *     https://github.com/tukaani-project/xz
     *     https://github.com/tukaani-project/xz/blob/master/doc/xz-file-format.txt
     *
     *    Bzip2 format:
     *      https://www.sourceware.org/bzip2/
     *      https://gitlab.com/bzip2/bzip2/
     *      https://github.com/dsnet/compress/blob/master/doc/bzip2-format.pdf
     *        (GitHub won't render it; download and open it)
     *
     *    Lzip format:
     *      https://www.nongnu.org/lzip/
     */

    /*
     * We didn't see anything that looks like a header for any type of
     * compressed file that we support, so just do uncompressed I/O.
     *
     * XXX - This fast seek data is for the case where a compressed stream
     * ends and is followed by an uncompressed portion.  It only works if
     * the uncompressed portion is at the end, as we don't constantly scan
     * for magic bytes in the middle of uncompressed data. (Concatenated
     * compressed streams _do_ work, even streams of different compression types.)
     */
    if (state->fast_seek)
        fast_seek_header(state, state->raw_pos - state->in.avail, state->pos, UNCOMPRESSED);


    /* doing raw i/o, save start of raw data for seeking, copy any leftover
       input to output -- this assumes that the output buffer is larger than
       the input buffer, which also assures space for gzungetc() */
    state->raw = state->pos;
    state->out.next = state->out.buf;
    /* not a compressed file -- copy everything we've read into the
       input buffer to the output buffer and fall to raw i/o */
    if (state->in.avail) {
        memcpy(state->out.buf, state->in.next, state->in.avail);
        state->out.avail = state->in.avail;

        /* Now discard everything in the input buffer */
        buf_reset(&state->in);
    }
    state->compression = UNCOMPRESSED;
    return 0;
}

/*
 * Based on what gz_make() in zlib does.
 */
static int
fill_out_buffer(FILE_T state)
{
    if (state->compression == UNKNOWN) {
        /*
         * We don't yet know whether the file is compressed,
         * so check for a compressed-file header.
         */
        if (check_for_compression(state) == -1)
            return -1;
        if (state->out.avail != 0)                /* got some data from check_for_compression() */
            return 0;
    }

    /*
     * We got no data from check_for_compression(), or we didn't call
     * it as we already know the compression type, so read some more
     * data.
     */
    switch (state->compression) {

    case UNCOMPRESSED:
        /* straight copy */
        if (!uncompressed_fill_out_buffer(state))
            return -1;
        break;

    default:
        break;
    }
    return 0;
}

static int
gz_skip(FILE_T state, int64_t len)
{
    unsigned n;

    /* skip over len bytes or reach end-of-file, whichever comes first */
    while (len)
        if (state->out.avail != 0) {
            /* We have stuff in the output buffer; skip over
               it. */
            n = (int64_t)state->out.avail > len ? (unsigned)len : state->out.avail;
            state->out.avail -= n;
            state->out.next += n;
            state->pos += n;
            len -= n;
        } else if (state->err != 0) {
            /* We have nothing in the output buffer, and
               we have an error that may not have been
               reported yet; that means we can't generate
               any more data into the output buffer, so
               return an error indication. */
            return -1;
        } else if (state->eof && state->in.avail == 0) {
            /* We have nothing in the output buffer, and
               we're at the end of the input; just return. */
            break;
        } else {
            /* We have nothing in the output buffer, and
               we can generate more data; get more output,
               looking for header if required. */
            if (fill_out_buffer(state) == -1)
                return -1;
        }
    return 0;
}

static void
gz_reset(FILE_T state)
{
    buf_reset(&state->out);       /* no output data available */
    state->eof = false;           /* not at end of file */
    state->compression = UNKNOWN; /* look for compression header */

    state->seek_pending = false;  /* no seek request pending */
    state->err = 0;               /* clear error */
    state->err_info = NULL;
    state->pos = 0;               /* no uncompressed data yet */
    buf_reset(&state->in);        /* no input data yet */
}

FILE_T
file_fdopen(int fd)
{
    /*
     * XXX - we now check whether we have st_blksize in struct stat;
     * it's not available on all platforms.
     *
     * I'm not sure why we're testing _STATBUF_ST_BLKSIZE; it's not
     * set on all platforms that have st_blksize in struct stat.
     * (Not all platforms have st_blksize in struct stat.)
     *
     * Is there some reason *not* to make the buffer size the maximum
     * of GBUFSIZE and st_blksize?  On most UN*Xes, the standard I/O
     * library does I/O with st_blksize as the buffer size; on others,
     * and on Windows, it's a 4K buffer size.  If st_blksize is bigger
     * than GBUFSIZE (which is currently 4KB), that's probably a
     * hint that reading in st_blksize chunks is considered a good
     * idea (e.g., an 8K/1K Berkeley fast file system with st_blksize
     * being 8K, or APFS, where st_blksize is big on at least some
     * versions of macOS).
     */
#ifdef _STATBUF_ST_BLKSIZE
    ws_statb64 st;
#endif /* _STATBUF_ST_BLKSIZE */
    unsigned want = GZBUFSIZE;
    FILE_T state;

    if (fd == -1)
        return NULL;

    /* allocate FILE_T structure to return */
    state = (FILE_T)g_try_malloc0(sizeof *state);
    if (state == NULL)
        return NULL;

    state->fast_seek_cur = NULL;
    state->fast_seek = NULL;

    /* open the file with the appropriate mode (or just use fd) */
    state->fd = fd;

    /* we don't yet know whether it's compressed */
    state->is_compressed = false;
    state->last_compression = UNKNOWN;

    /* save the current position for rewinding (only if reading) */
    state->start = ws_lseek64(state->fd, 0, SEEK_CUR);
    if (state->start == -1) state->start = 0;
    state->raw_pos = state->start;

    /* initialize stream */
    gz_reset(state);

#ifdef _STATBUF_ST_BLKSIZE
    /*
     * See what I/O size the file system recommends using, and if
     * it's bigger than what we're using and isn't too big, use
     * it.
     */
    if (ws_fstat64(fd, &st) >= 0) {
        /*
         * Yes, st_blksize can be bigger than an int; apparently,
         * it's a long on LP64 Linux, for example.
         *
         * If the value is too big to fit into a unsigned,
         * just use the maximum read buffer size.
         *
         * On top of that, the Single UNIX Speification says that
         * st_blksize is of type blksize_t, which is a *signed*
         * integer type, and, at minimum, macOS 11.6 and Linux 5.14.11's
         * include/uapi/asm-generic/stat.h define it as such.
         *
         * However, other OSes might make it unsigned, and older versions
         * of OSes that currently make it signed might make it unsigned,
         * so we try to avoid warnings from that.
         *
         * We cast MAX_READ_BUF_SIZE to long in order to avoid the
         * warning, although it might introduce warnings on platforms
         * where st_blocksize is unsigned; we'll deal with that if
         * it ever shows up as an issue.
         *
         * MAX_READ_BUF_SIZE is < the largest *signed* 32-bt integer,
         * so casting it to long won't turn it into a negative number.
         * (We only support 32-bit and 64-bit 2's-complement platforms.)
         */
        if (st.st_blksize <= (long)MAX_READ_BUF_SIZE)
            want = (unsigned)st.st_blksize;
        else
            want = MAX_READ_BUF_SIZE;
        /* XXX, verify result? */
    }
#endif /* _STATBUF_ST_BLKSIZE */
    state->in.buf = (unsigned char *)g_try_malloc(want);
    state->in.next = state->in.buf;
    state->in.avail = 0;
    state->out.buf = (unsigned char *)g_try_malloc(want << 1);
    state->out.next = state->out.buf;
    state->out.avail = 0;
    state->size = want;
    if (state->in.buf == NULL || state->out.buf == NULL) {
       goto err;
    }

    return state;

err:
    g_free(state->out.buf);
    g_free(state->in.buf);
    g_free(state);
    errno = ENOMEM;
    return NULL;
}

FILE_T
file_open(const char *path)
{
    int fd;
    FILE_T ft;

    /* open file and do correct filename conversions.

       XXX - do we need O_LARGEFILE?  On UN*X, if we need to do
       something special to get large file support, the configure
       script should have set us up with the appropriate #defines,
       so we should be getting a large-file-enabled file descriptor
       here.  Pre-Large File Summit UN*Xes, and possibly even some
       post-LFS UN*Xes, might require O_LARGEFILE here, though.
       If so, we should probably handle that in ws_open(). */
    if ((fd = ws_open(path, O_RDONLY|O_BINARY, 0000)) == -1)
        return NULL;

    /* open file handle */
    ft = file_fdopen(fd);
    if (ft == NULL) {
        ws_close(fd);
        return NULL;
    }


    return ft;
}

void
file_set_random_access(FILE_T stream, bool random_flag , GPtrArray *seek)
{
    stream->fast_seek = seek;
}

int64_t
file_seek(FILE_T file, int64_t offset, int whence, int *err)
{
    struct fast_seek_point *here;
    unsigned n;

    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        ws_assert_not_reached();
/*
 *err = EINVAL;
 return -1;
*/
    }

    /* Normalize offset to a SEEK_CUR specification */
    if (whence == SEEK_END) {
        /* Seek relative to the end of the file; given that we might be
           reading from a compressed file, we do that by seeking to the
           end of the file, making an offset relative to the end of
           the file an offset relative to the current position.

           XXX - we don't actually use this yet, but, for uncompressed
           files, we could optimize it, if desired, by directly using
           ws_lseek64(). */
        if (gz_skip(file, INT64_MAX) == -1) {
            *err = file->err;
            return -1;
        }
        if (offset == 0) {
            /* We are done */
            return file->pos;
        }
    } else if (whence == SEEK_SET)
        offset -= file->pos;
    else if (file->seek_pending) {
        /* There's a forward-skip pending, so file->pos doesn't reflect
           the actual file position, it represents the position from
           which we're skipping; update the offset to include that. */
        offset += file->skip;
    }
    file->seek_pending = false;

    /*
     * Are we moving at all?
     */
    if (offset == 0) {
        /* No.  Just return the current position. */
        return file->pos;
    }

    /*
     * Are we seeking backwards?
     */
    if (offset < 0) {
        /*
         * Yes.
         *
         * Do we have enough data before the current position in the
         * buffer that we can seek backwards within the buffer?
         */
        if (-offset <= offset_in_buffer(&file->out)) {
            /*
             * Yes.  Adjust appropriately.
             *
             * offset is negative, so -offset is non-negative, and
             * -offset is <= an unsigned and thus fits in an unsigned.
             * Get that value and adjust appropriately.
             *
             * (Casting offset to unsigned makes it positive, which
             * is not what we would want, so we cast -offset instead.)
             *
             * XXX - this won't work with -offset = 2^63, as its
             * negative isn't a valid 64-bit integer, but we are
             * not at all likely to see files big enough to ever
             * see a negative offset that large.
             */
            unsigned adjustment = (unsigned)(-offset);

            file->out.avail += adjustment;
            file->out.next -= adjustment;
            file->pos -= adjustment;
            return file->pos;
        }
    } else {
        /*
         * No.  Offset is positive; we're seeking forwards.
         *
         * Do we have enough data after the current position in the
         * buffer that we can seek forwards within the buffer?
         */
        if (offset < file->out.avail) {
            /*
             * Yes.  Adjust appropriately.
             *
             * offset is < an unsigned and thus fits in an unsigned,
             * so we can cast it to unsigned safely.
             */
            file->out.avail -= (unsigned)offset;
            file->out.next += offset;
            file->pos += offset;
            return file->pos;
        }
    }

    /*
     * We're not seeking within the buffer.  Do we have "fast seek" data
     * for the location to which we will be seeking, and are we either
     * seeking backwards or is the fast seek point past what is in the
     * buffer? (We don't want to "fast seek" backwards to a point that
     * we've already read and buffered if we're actually seeking forwards.)
     *
     * It might in certain cases be faster to continue reading linearly
     * foward rather than jump to the fast seek point if the distance
     * to the fast seek point is small, but we might only be able to do that
     * if the compression context doesn't change (which for LZ4 includes if
     * we jump to a LZ4 with different options.)
     * XXX - profile different buffer and SPAN sizes
     */
    if ((here = fast_seek_find(file, file->pos + offset)) &&
        (offset < 0 || here->out >= file->pos + file->out.avail)) {
        int64_t off, off2;

        /*
         * Yes.  Use that data to do the seek.
         * Note that this will be true only if file_set_random_access()
         * has been called on this file, which should never be the case
         * for a pipe.
         */
        switch (here->compression) {

        case UNCOMPRESSED:
            /* In an uncompressed portion, seek directly to the offset */
            off2 = (file->pos + offset);
            off = here->in + (off2 - here->out);
            break;

        default:
            /* Otherwise, seek to the fast seek point to do any needed setup. */
            off = here->in;
            off2 = here->out;
            break;
        }

        if (ws_lseek64(file->fd, off, SEEK_SET) == -1) {
            *err = errno;
            return -1;
        }
        fast_seek_reset(file);

        file->raw_pos = off;
        buf_reset(&file->out);
        file->eof = false;
        file->seek_pending = false;
        file->err = 0;
        file->err_info = NULL;
        buf_reset(&file->in);

        switch (here->compression) {



        default:
            file->compression = here->compression;
            break;
        }

        offset = (file->pos + offset) - off2;
        file->pos = off2;
        if (offset) {
            file->seek_pending = true;
            file->skip = offset;
        }
        return file->pos + offset;
    }
    if (file->compression == UNCOMPRESSED && file->pos + offset >= file->raw
        && (offset < 0 || offset >= file->out.avail)
        && (file->fast_seek != NULL))
    {
        if (ws_lseek64(file->fd, offset - file->out.avail, SEEK_CUR) == -1) {
            *err = errno;
            return -1;
        }
        file->raw_pos += (offset - file->out.avail);
        buf_reset(&file->out);
        file->eof = false;
        file->seek_pending = false;
        file->err = 0;
        file->err_info = NULL;
        buf_reset(&file->in);
        file->pos += offset;
        return file->pos;
    }
    if (offset < 0) {
        offset += file->pos;
        if (offset < 0) {                    /* before start of file! */
            *err = EINVAL;
            return -1;
        }
        if (ws_lseek64(file->fd, file->start, SEEK_SET) == -1) {
            *err = errno;
            return -1;
        }
        fast_seek_reset(file);
        file->raw_pos = file->start;
        gz_reset(file);
    }
    n = (int64_t)file->out.avail > offset ? (unsigned)offset : file->out.avail;
    file->out.avail -= n;
    file->out.next += n;
    file->pos += n;
    offset -= n;
    if (offset) {
        file->seek_pending = true;
        file->skip = offset;
    }
    return file->pos + offset;
}

int64_t
file_tell(FILE_T stream)
{
    return stream->pos + (stream->seek_pending ? stream->skip : 0);
}

int64_t
file_tell_raw(FILE_T stream)
{
    return stream->raw_pos;
}

int
file_fstat(FILE_T stream, ws_statb64 *statb, int *err)
{
    if (ws_fstat64(stream->fd, statb) == -1) {
        if (err != NULL)
            *err = errno;
        return -1;
    }
    return 0;
}

bool
file_iscompressed(FILE_T stream)
{
    return stream->is_compressed;
}

/* Returns a wtap compression type. If we don't know the compression type,
 * return WTAP_UNCOMPRESSED, but if our compression state is temporarily
 * UNKNOWN because we need to reread compression headers, return the last
 * known compression type.
 */
static wtap_compression_type
file_get_compression_type(FILE_T stream)
{
    if (stream->is_compressed) {
        switch ((stream->compression == UNKNOWN) ? stream->last_compression : stream->compression) {

        case ZLIB:
        case GZIP_AFTER_HEADER:
            return WTAP_GZIP_COMPRESSED;

        case ZSTD:
            return WTAP_ZSTD_COMPRESSED;

        case LZ4:
            return WTAP_LZ4_COMPRESSED;

        case UNCOMPRESSED:
            return WTAP_UNCOMPRESSED;

        default: /* UNKNOWN, should never happen if is_compressed is set */
            ws_assert_not_reached();
            return WTAP_UNCOMPRESSED;
        }
    }
    return WTAP_UNCOMPRESSED;
}

int
file_read(void *buf, unsigned int len, FILE_T file)
{
    unsigned got, n;

    /* if len is zero, avoid unnecessary operations */
    if (len == 0)
        return 0;

    /* process a skip request */
    if (file->seek_pending) {
        file->seek_pending = false;
        if (gz_skip(file, file->skip) == -1)
            return -1;
    }

    /*
     * Get len bytes to buf, or less than len if at the end;
     * if buf is null, just throw the bytes away.
     */
    got = 0;
    do {
        if (file->out.avail != 0) {
            /* We have stuff in the output buffer; copy
               what we have. */
            n = file->out.avail > len ? len : file->out.avail;
            if (buf != NULL) {
                memcpy(buf, file->out.next, n);
                buf = (char *)buf + n;
            }
            file->out.next += n;
            file->out.avail -= n;
            len -= n;
            got += n;
            file->pos += n;
        } else if (file->err != 0) {
            /* We have nothing in the output buffer, and
               we have an error that may not have been
               reported yet; that means we can't generate
               any more data into the output buffer, so
               return an error indication. */
            return -1;
        } else if (file->eof && file->in.avail == 0) {
            /* We have nothing in the output buffer, and
               we're at the end of the input; just return
               with what we've gotten so far. */
            break;
        } else {
            /* We have nothing in the output buffer, and
               we can generate more data; get more output,
               looking for header if required, and
               keep looping to process the new stuff
               in the output buffer. */
            if (fill_out_buffer(file) == -1)
                return -1;
        }
    } while (len);

    return (int)got;
}

/*
 * XXX - this *peeks* at next byte, not a character.
 */
int
file_peekc(FILE_T file)
{
    int ret = 0;

    /* check that we're reading and that there's no error */
    if (file->err != 0)
        return -1;

    /* try output buffer (no need to check for skip request) */
    if (file->out.avail != 0) {
        return *(file->out.next);
    }

    /* process a skip request */
    if (file->seek_pending) {
        file->seek_pending = false;
        if (gz_skip(file, file->skip) == -1)
            return -1;
    }
    /* if we processed a skip request, there may be data in the buffer,
     * or an error could have occurred; likewise if we didn't do seek but
     * now call fill_out_buffer, the errors can occur.  So we do this while
     * loop to check before and after - this is basically the logic from
     * file_read() but only for peeking not consuming a byte
     */
    while (1) {
        if (file->out.avail != 0) {
            return *(file->out.next);
        }
        else if (file->err != 0) {
            return -1;
        }
        else if (file->eof && file->in.avail == 0) {
            return -1;
        }
        else if (fill_out_buffer(file) == -1) {
            return -1;
        }
    }
    /* it's actually impossible to get here */
    return ret;
}

/*
 * XXX - this gets a byte, not a character.
 */
int
file_getc(FILE_T file)
{
    unsigned char buf[1];
    int ret;

    /* check that we're reading and that there's no error */
    if (file->err != 0)
        return -1;

    /* try output buffer (no need to check for skip request) */
    if (file->out.avail != 0) {
        file->out.avail--;
        file->pos++;
        return *(file->out.next)++;
    }

    ret = file_read(buf, 1, file);
    return ret < 1 ? -1 : buf[0];
}

/*
 * Like file_gets, but returns a pointer to the terminating NUL
 * on success and NULL on failure.
 */
char *
file_getsp(char *buf, int len, FILE_T file)
{
    unsigned left, n;
    char *curp;
    unsigned char *eol;

    /* check parameters */
    if (buf == NULL || len < 1)
        return NULL;

    /* check that there's no error */
    if (file->err != 0)
        return NULL;

    /* process a skip request */
    if (file->seek_pending) {
        file->seek_pending = false;
        if (gz_skip(file, file->skip) == -1)
            return NULL;
    }

    /* copy output bytes up to new line or len - 1, whichever comes first --
       append a terminating zero to the string (we don't check for a zero in
       the contents, let the user worry about that) */
    curp = buf;
    left = (unsigned)len - 1;
    if (left) do {
            /* assure that something is in the output buffer */
            if (file->out.avail == 0) {
                /* We have nothing in the output buffer. */
                if (file->err != 0) {
                    /* We have an error that may not have
                       been reported yet; that means we
                       can't generate any more data into
                       the output buffer, so return an
                       error indication. */
                    return NULL;
                }
                if (fill_out_buffer(file) == -1)
                    return NULL;            /* error */
                if (file->out.avail == 0)  {     /* end of file */
                    if (curp == buf)        /* got bupkus */
                        return NULL;
                    break;                  /* got something -- return it */
                }
            }

            /* look for end-of-line in current output buffer */
            n = file->out.avail > left ? left : file->out.avail;
            eol = (unsigned char *)memchr(file->out.next, '\n', n);
            if (eol != NULL)
                n = (unsigned)(eol - file->out.next) + 1;

            /* copy through end-of-line, or remainder if not found */
            memcpy(curp, file->out.next, n);
            file->out.avail -= n;
            file->out.next += n;
            file->pos += n;
            left -= n;
            curp += n;
        } while (left && eol == NULL);

    /* found end-of-line or out of space -- add a terminator and return
       a pointer to it */
    *curp = '\0';
    return curp;
}

/*
 * Returns a pointer to the beginning of the buffer on success
 * and NULL on failure.
 */
char *
file_gets(char *buf, int len, FILE_T file)
{
    if (!file_getsp(buf, len, file)) return NULL;
    return buf;
}

int
file_eof(FILE_T file)
{
    /* return end-of-file state */
    return (file->eof && file->in.avail == 0 && file->out.avail == 0);
}

/*
 * Routine to return a Wiretap error code (0 for no error, an errno
 * for a file error, or a WTAP_ERR_ code for other errors) for an
 * I/O stream.  Also returns an error string for some errors.
 */
int
file_error(FILE_T fh, char **err_info)
{
    if (fh->err!=0 && err_info) {
        /* g_strdup() returns NULL for NULL argument */
        *err_info = g_strdup(fh->err_info);
    }
    return fh->err;
}

void
file_clearerr(FILE_T stream)
{
    /* clear error and end-of-file */
    stream->err = 0;
    stream->err_info = NULL;
    stream->eof = false;
}

void
file_fdclose(FILE_T file)
{
    if (file->fd != -1)
        ws_close(file->fd);
    file->fd = -1;
}

bool
file_fdreopen(FILE_T file, const char *path)
{
    int fd;

    if ((fd = ws_open(path, O_RDONLY|O_BINARY, 0000)) == -1)
        return false;
    file->fd = fd;
    return true;
}

void
file_close(FILE_T file)
{
    int fd = file->fd;

    /* free memory and close file */
    if (file->size) {
        g_free(file->out.buf);
        g_free(file->in.buf);
    }
    g_free(file->fast_seek_cur);
    file->err = 0;
    file->err_info = NULL;
    g_free(file);
    /*
     * If fd is -1, somebody's done a file_closefd() on us, so
     * we don't need to close the FD itself, and shouldn't do
     * so.
     */
    if (fd != -1)
        ws_close(fd);
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
