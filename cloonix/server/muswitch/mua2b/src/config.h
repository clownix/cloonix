/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#define MAX_SAMPLES 6000

enum{
  input_cmd_none = 0,
  input_cmd_delay,
  input_cmd_loss,
  input_cmd_qsize,
  input_cmd_bsize,
  input_cmd_rate,
  input_cmd_dump_config,
};
enum{
  input_dir_none = 0,
  input_dir_a2b,
  input_dir_b2a,
};
/*---------------------------------------------------------------------------*/
typedef struct t_connect_side
{
  int llid;
  long long enqueue;
  long long dequeue;
  long long dropped;
  long long tockens;
  long long stored;
  long long lost;
  long long conf_loss;
  long long conf_delay;
  long long conf_qsize;
  long long conf_bsize;
  long long conf_brate;
} t_connect_side;
/*---------------------------------------------------------------------------*/
t_connect_side *get_sideA(void);
t_connect_side *get_sideB(void);
void config_fill_resp(char *resp, int max_len);
int config_recv_command(int input_cmd, int input_dir, int val);
void config_init(void);
/*---------------------------------------------------------------------------*/

