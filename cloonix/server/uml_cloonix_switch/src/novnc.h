/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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


#define CONFIG_NGINX "\n"\
"user www;\n"\
"worker_processes 1;\n"\
"daemon off;\n"\
"events {}\n"\
"http {\n"\
"    root /usr/libexec/cloonix/common/share/noVNC;\n"\
"    upstream vnc_proxy {\n"\
"        server 127.0.0.1:%s;\n"\
"    }\n"\
"    server {\n"\
"        listen %s;\n"\
"        listen unix:%s/proxy_%s_pweb.sock;\n"\
"        location / {\n"\
"            index vnc_lite.html;\n"\
"            try_files $uri /vnc_lite.html;\n"\
"            proxy_http_version 1.1;\n"\
"            proxy_pass http://vnc_proxy/;\n"\
"            proxy_set_header Upgrade $http_upgrade;\n"\
"            proxy_set_header Connection \"upgrade\";\n"\
"            proxy_read_timeout 61s;\n"\
"            proxy_buffering off;\n"\
"        }\n"\
"    }\n"\
"}\n"

int start_novnc(void);
int end_novnc(int terminate);
void init_novnc(char *net_name, int rank, int port);
/*--------------------------------------------------------------------------*/


