/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void llid_traf_tx_to_client(char *name, int dido_llid,  
                            int len, int type, int val, 
                            char  *buf);
void llid_traf_delete(int dido_llid);
void llid_traf_client_gone(int dido_llid);
void llid_traf_rx_from_client(int dido_llid, int len, char *buf);
void llid_traf_heartbeat(void);

