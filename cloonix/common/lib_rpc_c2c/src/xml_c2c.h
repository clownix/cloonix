/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#define MAX_CLOWNIX_BOUND_LEN      64
#define MIN_CLOWNIX_BOUND_LEN      2
/*---------------------------------------------------------------------------*/
#define C2C_PEER_CREATE  "<c2c_peer_create>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <master_cloonix> %s </master_cloonix>\n"\
                         "  <master_idx> %d </master_idx>\n"\
                         "  <ip_c2c_slave> %d </ip_c2c_slave>\n"\
                         "  <port_c2c_slave> %d </port_c2c_slave>\n"\
                         "</c2c_peer_create>\n"
/*---------------------------------------------------------------------------*/
#define C2C_ACK_PEER_CREATE     "<c2c_ack_peer_create>\n"\
                                "  <tid> %d </tid>\n"\
                                "  <name> %s </name>\n"\
                                "  <master_cloonix> %s </master_cloonix>\n"\
                                "  <slave_cloonix> %s </slave_cloonix>\n"\
                                "  <master_idx> %d </master_idx>\n"\
                                "  <slave_idx> %d </slave_idx>\n"\
                                "  <status> %d </status>\n"\
                                "</c2c_ack_peer_create>\n"
/*---------------------------------------------------------------------------*/
#define C2C_PEER_PING           "<c2c_ping>\n"\
                                "  <tid> %d </tid>\n"\
                                "  <name> %s </name>\n"\
                                "</c2c_ping>\n"
/*---------------------------------------------------------------------------*/
#define C2C_PEER_DELETE         "<c2c_peer_delete>\n"\
                                "  <tid> %d </tid>\n"\
                                "  <name> %s </name>\n"\
                                "</c2c_peer_delete>\n"
/*---------------------------------------------------------------------------*/





