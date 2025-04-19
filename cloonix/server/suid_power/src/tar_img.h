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
int tar_img_check(char *bulk, char *image, int is_persistent, char *brandtype);
char *tar_img_rootfs_get(char *name, char *bulk, char *image, char *brandtype);
int tar_img_add(char *name, char *bulk, char *image,
                 char *cnt_dir, int is_persistent, char *brandtype);
int tar_img_del(char *name, char *bulk, char *image,
                 char *cnt_dir, int is_persistent, char *brandtype);
int tar_img_exists(char *bulk, char *image, char *brandtype);
void tar_img_init(void);
/*--------------------------------------------------------------------------*/
