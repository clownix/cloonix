/** @file
 * Socket wrappers
 *
 * Copyright 2016, Dario Lombardo
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <wireshark.h>

#include <unistd.h>
#include <sys/socket.h>

	#define closesocket(socket)	close(socket)
	#define socket_handle_t		int
#ifndef INVALID_SOCKET
	#define INVALID_SOCKET		(-1)
#endif
	#define SOCKET_ERROR		(-1)

#include <arpa/inet.h>

#include <netinet/in.h>

#ifdef	__cplusplus
extern "C" {
#endif

int ws_socket_ptoa(struct sockaddr_storage *dst, const char *src, uint16_t def_port);

#ifdef	__cplusplus
}
#endif

#endif /* __SOCKET_H__ */

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
