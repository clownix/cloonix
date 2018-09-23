/****************************************************************************/
void channel_beat_set (t_all_ctx *all_ctx, t_heartbeat_cb beat);
/*---------------------------------------------------------------------------*/
int channel_delete(t_all_ctx *all_ctx, int llid);
/*---------------------------------------------------------------------------*/
void channel_loop(t_all_ctx *all_ctx);
/*---------------------------------------------------------------------------*/
void channel_init(t_all_ctx *all_ctx);
/*---------------------------------------------------------------------------*/
int channel_check_llid(t_all_ctx *all_ctx, int llid, const char *fct);
/*---------------------------------------------------------------------------*/
void channel_rx_local_flow_ctrl(void *ptr, int llid, int stop);
void channel_tx_local_flow_ctrl(void *ptr, int llid, int stop);
/*---------------------------------------------------------------------------*/







