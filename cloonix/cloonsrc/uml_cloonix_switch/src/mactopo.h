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
int mactopo_test_exists(char *name, int num);
int mactopo_add_req(int item_type, char *name, int num, char *lan,
                    char *vhost, unsigned char *mac_addr, char *err);
void mactopo_snf_add(char *name, int num, char *lan, int llid);
void mactopo_snf_del(char *name, int num, char *lan, int llid);
void mactopo_del_req(int item_type, char *name, int num, char *lan);
void mactopo_add_resp(int item_type, char *name, int num, char *lan);
void mactopo_del_resp(int item_type, char *name, int num, char *lan);
void mactopo_init(void);
