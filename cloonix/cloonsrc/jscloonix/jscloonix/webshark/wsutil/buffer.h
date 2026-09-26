/** @file
 *
 * Wiretap Library
 * Copyright (c) 1998 by Gilbert Ramirez <gram@alumni.rice.edu>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __W_BUFFER_H__
#define __W_BUFFER_H__

#include <inttypes.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define SOME_FUNCTIONS_ARE_DEFINES

typedef struct Buffer {
	uint8_t	*data;
	size_t	allocated;
	size_t	start;
	size_t	first_free;
} Buffer;

void ws_buffer_init(Buffer* buffer, size_t space);
void ws_buffer_free(Buffer* buffer);
void ws_buffer_assure_space(Buffer* buffer, size_t space);
void ws_buffer_append(Buffer* buffer, uint8_t *from, size_t bytes);
void ws_buffer_remove_start(Buffer* buffer, size_t bytes);
void ws_buffer_cleanup(void);

#ifdef SOME_FUNCTIONS_ARE_DEFINES
# define ws_buffer_clean(buffer) ws_buffer_remove_start((buffer), ws_buffer_length(buffer))
# define ws_buffer_increase_length(buffer,bytes) (buffer)->first_free += (bytes)
# define ws_buffer_length(buffer) ((buffer)->first_free - (buffer)->start)
# define ws_buffer_start_ptr(buffer) ((buffer)->data + (buffer)->start)
# define ws_buffer_end_ptr(buffer) ((buffer)->data + (buffer)->first_free)
# define ws_buffer_append_buffer(buffer,src_buffer) ws_buffer_append((buffer), ws_buffer_start_ptr(src_buffer), ws_buffer_length(src_buffer))
#else
 void ws_buffer_clean(Buffer* buffer);
 void ws_buffer_increase_length(Buffer* buffer, size_t bytes);
 size_t ws_buffer_length(Buffer* buffer);
 uint8_t* ws_buffer_start_ptr(Buffer* buffer);
 uint8_t* ws_buffer_end_ptr(Buffer* buffer);
 void ws_buffer_append_buffer(Buffer* buffer, Buffer* src_buffer);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
