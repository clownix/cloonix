/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#define LAYOUT_MOVE_ON_OFF "<layout_move_on_off>\n"\
                          "<tid> %d </tid>\n"\
                          "<on> %d </on>\n"\
                          "</layout_move_on_off>"
/*---------------------------------------------------------------------------*/
#define LAYOUT_WIDTH_HEIGHT "<layout_width_height>\n"\
                            "<tid> %d </tid>\n"\
                            "<width> %d </width>\n"\
                            "<height> %d </height>\n"\
                            "</layout_width_height>"
/*---------------------------------------------------------------------------*/
#define LAYOUT_CENTER_SCALE "<layout_center_scale>\n"\
                            "<tid> %d </tid>\n"\
                            "<cx> %d </cx>\n"\
                            "<cy> %d </cy>\n"\
                            "<cw> %d </cw>\n"\
                            "<ch> %d </ch>\n"\
                            "</layout_center_scale>"
/*---------------------------------------------------------------------------*/
#define LAYOUT_SAVE_PARAMS_RESP "<layout_save_params>\n"\
                                "<tid> %d </tid>\n"\
                                "<on> %d </on>\n"\
                                "<width> %d </width>\n"\
                                "<height> %d </height>\n"\
                                "<cx> %d </cx>\n"\
                                "<cy> %d </cy>\n"\
                                "<cw> %d </cw>\n"\
                                "<ch> %d </ch>\n"\
                                "</layout_save_params>"
/*---------------------------------------------------------------------------*/
#define LAYOUT_SAVE_PARAMS_REQ "<layout_req_save_params>\n"\
                               "<tid> %d </tid>\n"\
                               "</layout_req_save_params>"
/*---------------------------------------------------------------------------*/
#define LAYOUT_ZOOM   "<layout_zoom>\n"\
                      "<tid> %d </tid>\n"\
                      "<zoom> %d </zoom>\n"\
                      "</layout_zoom>"
/*---------------------------------------------------------------------------*/
#define LAYOUT_EVENT_SUB  "<layout_event_sub>\n"\
                          "<tid> %d </tid>\n"\
                          "<on> %d </on>\n"\
                          "</layout_event_sub>"
/*---------------------------------------------------------------------------*/
#define LAYOUT_LAN  "<layout_lan>\n"\
                    "<tid> %d </tid>\n"\
                    "<name> %s </name>\n"\
                    "<x> %d.%d </x>\n"\
                    "<y> %d.%d </y>\n"\
                    "<hidden> %d </hidden>\n"\
                    "</layout_lan>"
/*---------------------------------------------------------------------------*/
#define LAYOUT_SAT  "<layout_sat>\n"\
                    "<tid> %d </tid>\n"\
                    "<name> %s </name>\n"\
                    "<x> %d.%d </x>\n"\
                    "<y> %d.%d </y>\n"\
                    "<xa> %d.%d </xa>\n"\
                    "<ya> %d.%d </ya>\n"\
                    "<xb> %d.%d </xb>\n"\
                    "<yb> %d.%d </yb>\n"\
                    "<hidden> %d </hidden>\n"\
                    "</layout_sat>"
/*---------------------------------------------------------------------------*/
#define LAYOUT_MODIF "<layout_modif>\n"\
                     "<tid> %d </tid>\n"\
                     "<type> %d </type>\n"\
                     "<name> %s </name>\n"\
                     "<num> %d </num>\n"\
                     "<val1> %d </val1>\n"\
                     "<val2> %d </val2>\n"\
                     "</layout_modif>"
/*---------------------------------------------------------------------------*/
#define LAYOUT_INTF  "<eth_xyh> %d.%d %d.%d %d </eth_xyh>\n"



#define LAYOUT_NODE_O "<layout_node>\n"\
                      "<tid> %d </tid>\n"\
                      "<name> %s </name>\n"\
                      "<x> %d.%d </x>\n"\
                      "<y> %d.%d </y>\n"\
                      "<hidden> %d </hidden>\n"\
                      "<color> %d </color>\n"\
                      "<nb_eth> %d </nb_eth>  \n"

#define LAYOUT_NODE_C \
                     "</layout_node>"
/*---------------------------------------------------------------------------*/







