/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
void blkd_addr_print(char *str, uint8_t *addr);
void blkd_addr_hwaddr_rx(uint8_t *our_hwaddr, uint8_t *our_addr);
int blkd_addr_get_dst(uint8_t *hwaddr, uint8_t *addr, uint8_t *dst, int *idx); 
void blkd_addr_tx_virtio(int idx, int len, char *payload);
void blkd_addr_heartbeat(void);
int blkd_addr_init(int nb_idx);

