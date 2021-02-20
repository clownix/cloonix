/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#define C2C_REQ_IDX   "<c2c_req_idx>\n"\
                      "  <tid>  %d </tid>\n"\
                      "  <name> %s </name>\n"\
                      "</c2c_req_idx>\n"
/*---------------------------------------------------------------------------*/
#define C2C_RESP_IDX  "<c2c_resp_idx>\n"\
                      "  <tid>  %d </tid>\n"\
                      "  <name> %s </name>\n"\
                      "  <local_idx>  %d </local_idx>\n"\
                      "</c2c_resp_idx>\n"
/*---------------------------------------------------------------------------*/
#define C2C_REQ_CONX  "<c2c_req_conx>\n"\
                      "  <tid>  %d </tid>\n"\
                      "  <name> %s </name>\n"\
                      "  <peer_idx>  %d </peer_idx>\n"\
                      "  <peer_ip>  %d </peer_ip>\n"\
                      "  <peer_port> %d </peer_port>\n"\
                      "  <passwd> %s </passwd>\n"\
                      "</c2c_req_conx>\n"
/*---------------------------------------------------------------------------*/
#define C2C_RESP_CONX  "<c2c_resp_conx>\n"\
                       "  <tid>  %d </tid>\n"\
                       "  <name> %s </name>\n"\
                       "  <fd>  %d </fd>\n"\
                       "  <status>  %d </status>\n"\
                       "</c2c_resp_conx>\n"
/*---------------------------------------------------------------------------*/
#define C2C_REQ_FREE  "<c2c_req_free>\n"\
                      "  <tid>  %d </tid>\n"\
                      "  <name> %s </name>\n"\
                      "</c2c_req_free>\n"
/*---------------------------------------------------------------------------*/
#define C2C_CLONE_BIRTH "<c2c_clone_birth>\n"\
                        "  <tid>  %d </tid>\n"\
                        "  <net_name> %s </net_name>\n"\
                        "  <name> %s </name>\n"\
                        "  <fd>  %d </fd>\n"\
                        "  <endp_type>  %d </endp_type>\n"\
                        "  <bin_path> %s </bin_path>\n"\
                        "  <sock> %s </sock>\n"\
                        "</c2c_clone_birth>\n"
/*---------------------------------------------------------------------------*/
#define C2C_CLONE_BIRTH_PID "<c2c_clone_birth_pid>\n"\
                            "  <tid>  %d </tid>\n"\
                            "  <name> %s </name>\n"\
                            "  <pid>  %d </pid>\n"\
                            "</c2c_clone_birth_pid>\n"
/*---------------------------------------------------------------------------*/
#define C2C_CLONE_DEATH "<c2c_clone_death>\n"\
                        "  <tid>  %d </tid>\n"\
                        "  <name> %s </name>\n"\
                        "</c2c_clone_death>\n"
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
#define INFO_O  "<doors_any_info>\n"\
                 "  <tid>  %d </tid>\n"\
                 "  <info> %s </info>\n"

#define INFO_I  "  <doors_info_delimiter>%s</doors_info_delimiter>\n"

#define INFO_C  "</doors_any_info>"
/*---------------------------------------------------------------------------*/
#define EVENT_O  "<doors_event>\n"\
                 "  <tid>  %d </tid>\n"\
                 "  <name> %s </name>\n"

#define EVENT_I  "  <doors_event_delimiter>%s</doors_event_delimiter>\n"

#define EVENT_C  "</doors_event>"
/*---------------------------------------------------------------------------*/
#define STATUS_O  "<doors_status>\n"\
                  "  <tid>  %d </tid>\n"\
                  "  <name> %s </name>\n"

#define STATUS_I  "  <doors_status_delimiter>%s</doors_status_delimiter>\n"

#define STATUS_C  "</doors_status>"
/*---------------------------------------------------------------------------*/
#define COMMAND_O  "<doors_command>\n"\
                   "  <tid>  %d </tid>\n"\
                   "  <name> %s </name>\n"

#define COMMAND_I  "  <doors_command_delimiter>%s</doors_command_delimiter>\n"

#define COMMAND_C  "</doors_command>"
/*---------------------------------------------------------------------------*/
#define ADD_VM     "<doors_add_vm>\n"\
                   "  <tid>  %d </tid>\n"\
                   "  <name> %s </name>\n"\
                   "  <way2agent> %s </way2agent>\n"\
                   "</doors_add_vm>"
/*---------------------------------------------------------------------------*/
#define DEL_VM     "<doors_del_vm>\n"\
                   "  <tid>  %d </tid>\n"\
                   "  <name> %s </name>\n"\
                   "</doors_del_vm>"
/*---------------------------------------------------------------------------*/







