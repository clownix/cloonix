/****************************************************************************/
void receive_traf_x11_from_doors(int dido_llid, int sub_dido_idx, 
                                 int len, char *buf);

void send_traf_x11_to_doors(int dido_llid, int sub_dido_idx, 
                            int len, char *buf);

void receive_ctrl_x11_from_doors(int dido_llid, int doors_val, 
                                 int len, char *buf);

void send_ctrl_x11_to_doors(int dido_llid, int doors_val, 
                            int len, char *buf);
/*--------------------------------------------------------------------------*/


