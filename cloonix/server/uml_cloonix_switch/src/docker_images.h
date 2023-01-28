/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
typedef struct t_image_doc
{
  char brandtype[MAX_NAME_LEN];
  char image_name[MAX_NAME_LEN];
  struct t_image_doc *next;
} t_image_doc;
/*--------------------------------------------------------------------------*/
void add_slave_image_doc(char *brandtype, char *image_name);
void clean_slave_image_chain(void);
void swap_slave_master_image(void);
int docker_images_exists(char *brandtype, char *image_name);
int docker_image_get_list(char *brandname, t_slowperiodic **slowperiodic);
void docker_images_init(void);
/*--------------------------------------------------------------------------*/

