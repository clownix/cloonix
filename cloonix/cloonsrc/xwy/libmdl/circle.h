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
#define CIRC_FREE_MASK (0xFFF)
#define CIRC_TX_MASK (0xFFFF)

/*****************************************************************************/
typedef struct t_circ_free_queue
{
  void *elem;
} t_circ_free_queue;
/*---------------------------------------------------------------------------*/
typedef struct t_circ_tx_queue
{
  uint32_t bytes;
  void *elem;
} t_circ_tx_queue;
/*---------------------------------------------------------------------------*/
typedef struct t_circ_ctx
{

  uint32_t volatile free_head;
  uint32_t volatile free_tail;
  uint32_t volatile free_lock;
  t_circ_free_queue free_queue[CIRC_FREE_MASK+1];

  uint32_t volatile tx_head;
  uint32_t volatile tx_tail;
  uint32_t volatile tx_stored_bytes;
  uint32_t volatile tx_lock;
  t_circ_tx_queue tx_queue[CIRC_TX_MASK+1];

} t_circ_ctx;
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void  circ_free_slot_put(t_circ_ctx *circ_ctx, void *elem);
void *circ_free_slot_get(t_circ_ctx *circ_ctx);

void  circ_tx_slot_put(t_circ_ctx *circ_ctx, void *elem, int bytes);
void *circ_tx_slot_get(t_circ_ctx *circ_ctx);
int   circ_tx_empty_slot_nb(t_circ_ctx *circ_ctx);
int   circ_tx_used_slot_nb(t_circ_ctx *circ_ctx);
int   circ_tx_stored_bytes_nb(t_circ_ctx *circ_ctx);
void  circ_slot_init(t_circ_ctx *circ_ctx);
/*--------------------------------------------------------------------------*/
