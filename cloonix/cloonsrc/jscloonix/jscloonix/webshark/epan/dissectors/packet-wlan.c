/* packet-wlan.c
 * Radiotap, IEEE 802.11 and EAPOL for webshark: the frames of the wifi air
 * of a cloonix net (mac80211_hwsim radios seen on hwsim0). Written for this
 * trimmed epan; the protocol and field names are those of the wireshark
 * dissectors (radiotap.*, wlan.*, wlan.fixed.*, wlan.tag.*, wlan.rsn.*,
 * eapol.*), so a display filter written for wireshark works here.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include <epan/packet.h>
#include <epan/etypes.h>
#include <epan/to_str.h>
#include <epan/address.h>
#include <epan/unit_strings.h>
#include <wsutil/array.h>
#include <wiretap/wtap.h>

void proto_register_wlan(void);
void proto_reg_handoff_wlan(void);

static int proto_radiotap;
static int proto_wlan;
static int proto_eapol;

static int hf_rt_version, hf_rt_len, hf_rt_present, hf_rt_tsft, hf_rt_flags;
static int hf_rt_rate, hf_rt_freq, hf_rt_chflags, hf_rt_dbm_signal;
static int hf_rt_dbm_noise, hf_rt_antenna, hf_rt_fcs_flag;

static int hf_fc, hf_fc_type_subtype, hf_fc_type, hf_fc_subtype, hf_fc_flags;
static int hf_fc_tods, hf_fc_fromds, hf_fc_frag, hf_fc_retry, hf_fc_pwrmgt;
static int hf_fc_moredata, hf_fc_protected, hf_fc_order;
static int hf_duration, hf_ra, hf_ta, hf_da, hf_sa, hf_bssid, hf_addr;
static int hf_seq, hf_frag, hf_qos, hf_qos_tid, hf_fcs;

static int hf_ts, hf_bi, hf_cap, hf_cap_ess, hf_cap_ibss, hf_cap_privacy;
static int hf_cap_short_preamble, hf_cap_short_slot, hf_listen, hf_status;
static int hf_aid, hf_cur_ap, hf_auth_alg, hf_auth_seq, hf_reason;
static int hf_action_cat;

static int hf_tag_number, hf_tag_length, hf_ssid, hf_rate_mbps, hf_ds_channel;
static int hf_tim_dtim_count, hf_tim_dtim_period, hf_country;
static int hf_rsn_version, hf_rsn_gcs, hf_rsn_pcs, hf_rsn_akms, hf_rsn_cap;
static int hf_ht_primary, hf_vendor_oui, hf_vendor_type, hf_tag_data;

static int hf_ccmp_pn, hf_wep_data;
static int hf_llc_dsap, hf_llc_ssap, hf_llc_ctrl, hf_llc_oui, hf_llc_etype;

static int hf_eapol_version, hf_eapol_type, hf_eapol_len, hf_eapol_desc;
static int hf_eapol_keyinfo, hf_eapol_keylen, hf_eapol_replay;
static int hf_eapol_nonce, hf_eapol_mic, hf_eapol_keydata_len;

static int ett_rt, ett_wlan, ett_fc, ett_flags, ett_fixed, ett_tagged;
static int ett_tag, ett_cap, ett_qos, ett_llc, ett_eapol;

static dissector_handle_t radiotap_handle, wlan_handle, eapol_handle;
static dissector_handle_t ethertype_handle, data_handle;

#define FC_TYPE_MGT   0
#define FC_TYPE_CTRL  1
#define FC_TYPE_DATA  2

#define FLAG_TO_DS     0x01
#define FLAG_FROM_DS   0x02
#define FLAG_PROTECTED 0x40
#define FLAG_ORDER     0x80

#define RT_FLAG_FCS    0x10

/* type_subtype: type in the high nibble, subtype in the low one */
static const value_string type_subtype_vals[] = {
  { 0x00, "Association Request" },
  { 0x01, "Association Response" },
  { 0x02, "Reassociation Request" },
  { 0x03, "Reassociation Response" },
  { 0x04, "Probe Request" },
  { 0x05, "Probe Response" },
  { 0x06, "Timing Advertisement" },
  { 0x08, "Beacon frame" },
  { 0x09, "ATIM" },
  { 0x0a, "Disassociate" },
  { 0x0b, "Authentication" },
  { 0x0c, "Deauthentication" },
  { 0x0d, "Action" },
  { 0x0e, "Action No Ack" },
  { 0x14, "Beamforming Report Poll" },
  { 0x15, "VHT/HE NDP Announcement" },
  { 0x17, "Control Wrapper" },
  { 0x18, "802.11 Block Ack Req" },
  { 0x19, "802.11 Block Ack" },
  { 0x1a, "Power-Save poll" },
  { 0x1b, "Request-to-send" },
  { 0x1c, "Clear-to-send" },
  { 0x1d, "Acknowledgement" },
  { 0x1e, "CF-End" },
  { 0x1f, "CF-End + CF-Ack" },
  { 0x20, "Data" },
  { 0x24, "Null function (No data)" },
  { 0x28, "QoS Data" },
  { 0x2c, "QoS Null function (No data)" },
  { 0, NULL }
};

static const value_string type_vals[] = {
  { FC_TYPE_MGT,  "Management frame" },
  { FC_TYPE_CTRL, "Control frame" },
  { FC_TYPE_DATA, "Data frame" },
  { 3,            "Extension frame" },
  { 0, NULL }
};

static const value_string tag_vals[] = {
  {   0, "SSID parameter set" },
  {   1, "Supported Rates" },
  {   3, "DS Parameter set" },
  {   5, "Traffic Indication Map (TIM)" },
  {   7, "Country Information" },
  {  11, "QBSS Load Element" },
  {  42, "ERP Information" },
  {  45, "HT Capabilities (802.11n D1.10)" },
  {  48, "RSN Information" },
  {  50, "Extended Supported Rates" },
  {  54, "Mobility Domain" },
  {  59, "Supported Operating Classes" },
  {  61, "HT Information (802.11n D1.10)" },
  {  70, "RM Enabled Capabilities" },
  { 127, "Extended Capabilities" },
  { 191, "VHT Capabilities" },
  { 192, "VHT Operation" },
  { 221, "Vendor Specific" },
  { 255, "Element ID Extension" },
  { 0, NULL }
};

static const value_string auth_alg_vals[] = {
  { 0, "Open System" },
  { 1, "Shared key" },
  { 2, "Fast BSS Transition" },
  { 3, "Simultaneous Authentication of Equals (SAE)" },
  { 4, "FILS Shared Key authentication without PFS" },
  { 0, NULL }
};

static const value_string status_vals[] = {
  {  0, "Successful" },
  {  1, "Unspecified failure" },
  { 10, "Cannot support all requested capabilities" },
  { 12, "Association denied due to reason outside the scope of 802.11" },
  { 13, "Responding station does not support the specified authentication algorithm" },
  { 15, "Authentication rejected; the response to the challenge failed" },
  { 17, "Association denied; the AP is unable to handle additional associated STAs" },
  { 30, "Association request rejected temporarily; try again later" },
  { 0, NULL }
};

static const value_string reason_vals[] = {
  {  1, "Unspecified reason" },
  {  2, "Previous authentication no longer valid" },
  {  3, "Deauthenticated because sending STA is leaving (or has left) IBSS or ESS" },
  {  4, "Disassociated due to inactivity" },
  {  5, "Disassociated because AP is unable to handle all currently associated STAs" },
  {  6, "Class 2 frame received from nonauthenticated STA" },
  {  7, "Class 3 frame received from nonassociated STA" },
  {  8, "Disassociated because sending STA is leaving (or has left) BSS" },
  { 14, "Message integrity code (MIC) failure" },
  { 15, "4-Way Handshake timeout" },
  { 16, "Group key handshake timeout" },
  { 23, "IEEE 802.1X authentication failed" },
  { 0, NULL }
};

static const value_string action_cat_vals[] = {
  {  0, "Spectrum Management" },
  {  1, "QoS" },
  {  3, "Block Ack" },
  {  4, "Public Action" },
  {  5, "Radio Measurement" },
  {  7, "HT" },
  {  8, "SA Query" },
  { 10, "WNM" },
  { 21, "VHT" },
  { 0, NULL }
};

static const value_string rsn_cipher_vals[] = {
  { 0, "Use group cipher suite" },
  { 1, "WEP (40-bit)" },
  { 2, "TKIP" },
  { 4, "AES (CCM)" },
  { 5, "WEP (104-bit)" },
  { 6, "BIP (128)" },
  { 8, "GCMP (128)" },
  { 9, "GCMP (256)" },
  { 10, "CCMP (256)" },
  { 0, NULL }
};

static const value_string rsn_akm_vals[] = {
  { 1, "WPA (IEEE 802.1X)" },
  { 2, "PSK" },
  { 3, "FT over IEEE 802.1X" },
  { 4, "FT using PSK" },
  { 5, "WPA (SHA256)" },
  { 6, "PSK (SHA256)" },
  { 8, "SAE (SHA256)" },
  { 9, "FT using SAE (SHA256)" },
  { 18, "OWE" },
  { 0, NULL }
};

static const value_string eapol_type_vals[] = {
  { 0, "EAP Packet" },
  { 1, "Start" },
  { 2, "Logoff" },
  { 3, "Key" },
  { 4, "Encapsulated ASF Alert" },
  { 0, NULL }
};

static const value_string eapol_desc_vals[] = {
  { 1,   "RC4 Descriptor" },
  { 2,   "EAPOL RSN Key" },
  { 254, "EAPOL WPA Key" },
  { 0, NULL }
};

/* radiotap: alignment and size of each field, in the order of the bits */
typedef struct { int align; int size; } rt_field;
static const rt_field rt_fields[] = {
  { 8, 8 },  /*  0 TSFT */
  { 1, 1 },  /*  1 Flags */
  { 1, 1 },  /*  2 Rate */
  { 2, 4 },  /*  3 Channel */
  { 2, 2 },  /*  4 FHSS */
  { 1, 1 },  /*  5 dBm antenna signal */
  { 1, 1 },  /*  6 dBm antenna noise */
  { 2, 2 },  /*  7 Lock quality */
  { 2, 2 },  /*  8 TX attenuation */
  { 2, 2 },  /*  9 dB TX attenuation */
  { 1, 1 },  /* 10 dBm TX power */
  { 1, 1 },  /* 11 Antenna */
  { 1, 1 },  /* 12 dB antenna signal */
  { 1, 1 },  /* 13 dB antenna noise */
  { 2, 2 },  /* 14 RX flags */
  { 2, 2 },  /* 15 TX flags */
  { 1, 1 },  /* 16 RTS retries */
  { 1, 1 },  /* 17 data retries */
  { 4, 8 },  /* 18 XChannel */
  { 1, 3 },  /* 19 MCS */
  { 4, 8 },  /* 20 A-MPDU status */
  { 2, 12 }, /* 21 VHT */
  { 8, 12 }, /* 22 timestamp */
};
#define RT_NB_KNOWN ((int) (sizeof(rt_fields) / sizeof(rt_fields[0])))

static int dissect_radiotap(tvbuff_t *tvb, packet_info *pinfo,
                            proto_tree *tree, void *data)
{
  proto_item *ti;
  proto_tree *rt_tree;
  int len, off, bit, has_fcs = 0, nb_words = 1;
  uint32_t present, word;
  tvbuff_t *next;

  col_set_str(pinfo->cinfo, COL_PROTOCOL, "802.11");
  col_clear(pinfo->cinfo, COL_INFO);
  len = (tvb_captured_length(tvb) >= 4) ? tvb_get_letohs(tvb, 2) : -1;
  if ((tvb_captured_length(tvb) < 8) || (len < 8) ||
      (len > (int) tvb_captured_length(tvb)))
    {
    proto_tree_add_protocol_format(tree, proto_radiotap, tvb, 0, -1,
      "Radiotap Header: length %d not valid for a frame of %u bytes",
      len, tvb_captured_length(tvb));
    col_set_str(pinfo->cinfo, COL_INFO, "Malformed radiotap header");
    call_dissector(data_handle, tvb, pinfo, tree);
    return tvb_captured_length(tvb);
    }
  present = tvb_get_letohl(tvb, 4);
  ti = proto_tree_add_protocol_format(tree, proto_radiotap, tvb, 0, len,
                                      "Radiotap Header v%u, Length %d",
                                      tvb_get_uint8(tvb, 0), len);
  rt_tree = proto_item_add_subtree(ti, ett_rt);
  proto_tree_add_item(rt_tree, hf_rt_version, tvb, 0, 1, ENC_NA);
  proto_tree_add_item(rt_tree, hf_rt_len, tvb, 2, 2, ENC_LITTLE_ENDIAN);
  proto_tree_add_item(rt_tree, hf_rt_present, tvb, 4, 4, ENC_LITTLE_ENDIAN);
  /* extended bitmaps: bit 31 says another word follows */
  word = present;
  while ((word & 0x80000000) && (8 + nb_words * 4 <= len))
    {
    word = tvb_get_letohl(tvb, 4 + nb_words * 4);
    nb_words += 1;
    }
  off = 4 + nb_words * 4;
  for (bit = 0; bit < 31; bit++)
    {
    if (!(present & (1U << bit)))
      continue;
    /* past what is known here, the offsets of the rest cannot be computed */
    if (bit >= RT_NB_KNOWN)
      {
      proto_tree_add_subtree_format(rt_tree, tvb, off, len - off, ett_rt,
        NULL, "Radiotap field %d and after: not decoded here", bit);
      break;
      }
    off = (off + rt_fields[bit].align - 1) & ~(rt_fields[bit].align - 1);
    if (off + rt_fields[bit].size > len)
      {
      proto_tree_add_subtree_format(rt_tree, tvb, 0, len, ett_rt, NULL,
        "Radiotap field %d past the header length %d", bit, len);
      break;
      }
    switch (bit)
      {
      case 0:
        proto_tree_add_item(rt_tree, hf_rt_tsft, tvb, off, 8,
                            ENC_LITTLE_ENDIAN);
        break;
      case 1:
        proto_tree_add_item(rt_tree, hf_rt_flags, tvb, off, 1, ENC_NA);
        proto_tree_add_item(rt_tree, hf_rt_fcs_flag, tvb, off, 1, ENC_NA);
        has_fcs = (tvb_get_uint8(tvb, off) & RT_FLAG_FCS) ? 1 : 0;
        break;
      case 2:
        proto_tree_add_uint_format_value(rt_tree, hf_rt_rate, tvb, off, 1,
                                         tvb_get_uint8(tvb, off),
                                         "%.1f Mb/s",
                                         tvb_get_uint8(tvb, off) / 2.0);
        break;
      case 3:
        proto_tree_add_item(rt_tree, hf_rt_freq, tvb, off, 2,
                            ENC_LITTLE_ENDIAN);
        proto_tree_add_item(rt_tree, hf_rt_chflags, tvb, off + 2, 2,
                            ENC_LITTLE_ENDIAN);
        break;
      case 5:
        proto_tree_add_item(rt_tree, hf_rt_dbm_signal, tvb, off, 1, ENC_NA);
        break;
      case 6:
        proto_tree_add_item(rt_tree, hf_rt_dbm_noise, tvb, off, 1, ENC_NA);
        break;
      case 11:
        proto_tree_add_item(rt_tree, hf_rt_antenna, tvb, off, 1, ENC_NA);
        break;
      default:
        break;
      }
    off += rt_fields[bit].size;
    }
  next = tvb_new_subset_remaining(tvb, len);
  call_dissector_with_data(wlan_handle, next, pinfo, tree,
                           GINT_TO_POINTER(has_fcs));
  return tvb_captured_length(tvb);
}

static void add_llc(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                    proto_tree *wlan_tree, int off)
{
  proto_tree *llc_tree;
  proto_item *ti;
  uint16_t etype;
  ethertype_data_t ed;
  tvbuff_t *next;
  ti = proto_tree_add_protocol_format(tree, proto_wlan, tvb, off, 8,
         "Logical-Link Control");
  llc_tree = proto_item_add_subtree(ti, ett_llc);
  proto_tree_add_item(llc_tree, hf_llc_dsap, tvb, off, 1, ENC_NA);
  proto_tree_add_item(llc_tree, hf_llc_ssap, tvb, off + 1, 1, ENC_NA);
  proto_tree_add_item(llc_tree, hf_llc_ctrl, tvb, off + 2, 1, ENC_NA);
  proto_tree_add_item(llc_tree, hf_llc_oui, tvb, off + 3, 3,
                      ENC_BIG_ENDIAN);
  proto_tree_add_item(llc_tree, hf_llc_etype, tvb, off + 6, 2,
                      ENC_BIG_ENDIAN);
  etype = tvb_get_ntohs(tvb, off + 6);
  if (etype == ETHERTYPE_EAPOL)
    {
    next = tvb_new_subset_remaining(tvb, off + 8);
    call_dissector(eapol_handle, next, pinfo, tree);
    return;
    }
  memset(&ed, 0, sizeof(ed));
  ed.etype = etype;
  ed.payload_offset = off + 8;
  ed.fh_tree = wlan_tree;
  ed.trailer_id = -1;
  ed.fcs_len = 0;
  call_dissector_with_data(ethertype_handle, tvb, pinfo, tree, &ed);
}

/* The info column says what the tag says, the SSID first of all. */
static int add_tagged(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                      int off, int end)
{
  proto_tree *tags, *tag_tree;
  proto_item *ti;
  int id, len, i, n;
  char *ssid;
  if (off >= end)
    return off;
  tags = proto_tree_add_subtree(tree, tvb, off, end - off, ett_tagged, NULL,
                                "Tagged parameters");
  while (off + 2 <= end)
    {
    id = tvb_get_uint8(tvb, off);
    len = tvb_get_uint8(tvb, off + 1);
    if (off + 2 + len > end)
      {
      proto_tree_add_subtree_format(tags, tvb, off, end - off, ett_tag, NULL,
                                    "Tag %d: length %d past the frame", id,
                                    len);
      return end;
      }
    tag_tree = proto_tree_add_subtree_format(tags, tvb, off, 2 + len,
                 ett_tag, &ti, "Tag: %s", val_to_str_const(id, tag_vals,
                                                          "Reserved"));
    proto_tree_add_item(tag_tree, hf_tag_number, tvb, off, 1, ENC_NA);
    proto_tree_add_item(tag_tree, hf_tag_length, tvb, off + 1, 1, ENC_NA);
    off += 2;
    switch (id)
      {
      case 0:
        ssid = (char *) tvb_get_string_enc(pinfo->pool, tvb, off, len,
                                           ENC_UTF_8);
        proto_tree_add_item(tag_tree, hf_ssid, tvb, off, len, ENC_UTF_8);
        proto_item_append_text(ti, ": %s", len ? ssid : "Wildcard SSID");
        col_append_fstr(pinfo->cinfo, COL_INFO, ", SSID=\"%s\"",
                        len ? ssid : "Wildcard (Broadcast)");
        break;
      case 1:
      case 50:
        for (i = 0; i < len; i++)
          {
          n = tvb_get_uint8(tvb, off + i);
          proto_tree_add_uint_format_value(tag_tree, hf_rate_mbps, tvb,
                                           off + i, 1, n, "%.1f%s",
                                           (n & 0x7f) / 2.0,
                                           (n & 0x80) ? "(B)" : "");
          proto_item_append_text(ti, "%s%.1f%s", i ? ", " : " ",
                                 (n & 0x7f) / 2.0, (n & 0x80) ? "(B)" : "");
          }
        break;
      case 3:
        if (len >= 1)
          {
          proto_tree_add_item(tag_tree, hf_ds_channel, tvb, off, 1, ENC_NA);
          proto_item_append_text(ti, ": Current Channel: %u",
                                 tvb_get_uint8(tvb, off));
          }
        break;
      case 5:
        if (len >= 2)
          {
          proto_tree_add_item(tag_tree, hf_tim_dtim_count, tvb, off, 1,
                              ENC_NA);
          proto_tree_add_item(tag_tree, hf_tim_dtim_period, tvb, off + 1, 1,
                              ENC_NA);
          }
        break;
      case 7:
        if (len >= 2)
          proto_tree_add_item(tag_tree, hf_country, tvb, off, 2, ENC_ASCII);
        break;
      case 48:
        if (len >= 2)
          {
          int p = off, e = off + len, cnt;
          proto_tree_add_item(tag_tree, hf_rsn_version, tvb, p, 2,
                              ENC_LITTLE_ENDIAN);
          p += 2;
          if (p + 4 <= e)
            {
            proto_tree_add_uint(tag_tree, hf_rsn_gcs, tvb, p, 4,
                                tvb_get_uint8(tvb, p + 3));
            p += 4;
            }
          if (p + 2 <= e)
            {
            cnt = tvb_get_letohs(tvb, p);
            p += 2;
            for (i = 0; (i < cnt) && (p + 4 <= e); i++, p += 4)
              proto_tree_add_uint(tag_tree, hf_rsn_pcs, tvb, p, 4,
                                  tvb_get_uint8(tvb, p + 3));
            }
          if (p + 2 <= e)
            {
            cnt = tvb_get_letohs(tvb, p);
            p += 2;
            for (i = 0; (i < cnt) && (p + 4 <= e); i++, p += 4)
              {
              proto_tree_add_uint(tag_tree, hf_rsn_akms, tvb, p, 4,
                                  tvb_get_uint8(tvb, p + 3));
              proto_item_append_text(ti, " %s",
                val_to_str_const(tvb_get_uint8(tvb, p + 3), rsn_akm_vals,
                                 "AKM"));
              }
            }
          if (p + 2 <= e)
            proto_tree_add_item(tag_tree, hf_rsn_cap, tvb, p, 2,
                                ENC_LITTLE_ENDIAN);
          }
        break;
      case 61:
        if (len >= 1)
          proto_tree_add_item(tag_tree, hf_ht_primary, tvb, off, 1, ENC_NA);
        break;
      case 221:
        if (len >= 4)
          {
          proto_tree_add_item(tag_tree, hf_vendor_oui, tvb, off, 3,
                              ENC_BIG_ENDIAN);
          proto_tree_add_item(tag_tree, hf_vendor_type, tvb, off + 3, 1,
                              ENC_NA);
          if (tvb_get_ntoh24(tvb, off) == 0x0050f2)
            proto_item_append_text(ti, ": Microsoft Corp.: %s",
              (tvb_get_uint8(tvb, off + 3) == 1) ? "WPA Information Element" :
              (tvb_get_uint8(tvb, off + 3) == 2) ? "WMM/WME" : "type");
          }
        break;
      default:
        if (len)
          proto_tree_add_item(tag_tree, hf_tag_data, tvb, off, len, ENC_NA);
        break;
      }
    off += len;
    }
  return off;
}

static void add_cap(proto_tree *tree, tvbuff_t *tvb, int off)
{
  static int * const bits[] = {
    &hf_cap_ess, &hf_cap_ibss, &hf_cap_privacy, &hf_cap_short_preamble,
    &hf_cap_short_slot, NULL };
  proto_tree_add_bitmask(tree, tvb, off, hf_cap, ett_cap, bits,
                         ENC_LITTLE_ENDIAN);
}

static void dissect_mgt_body(tvbuff_t *tvb, packet_info *pinfo,
                             proto_tree *tree, int subtype, int off, int end)
{
  proto_tree *fixed;
  int flen = 0;
  switch (subtype)
    {
    case 0x0: flen = 4;  break;
    case 0x1: flen = 6;  break;
    case 0x2: flen = 10; break;
    case 0x3: flen = 6;  break;
    case 0x5:
    case 0x8: flen = 12; break;
    case 0xa:
    case 0xc: flen = 2;  break;
    case 0xb: flen = 6;  break;
    case 0xd:
    case 0xe: flen = 1;  break;
    default:  flen = 0;  break;
    }
  if (off + flen > end)
    {
    proto_tree_add_subtree_format(tree, tvb, off, end - off, ett_fixed, NULL,
      "Fixed parameters: %d bytes wanted, %d in the frame", flen, end - off);
    flen = 0;
    }
  if (flen)
    {
    fixed = proto_tree_add_subtree(tree, tvb, off, flen, ett_fixed, NULL,
                                   "Fixed parameters");
    switch (subtype)
      {
      case 0x0:
      case 0x2:
        add_cap(fixed, tvb, off);
        proto_tree_add_item(fixed, hf_listen, tvb, off + 2, 2,
                            ENC_LITTLE_ENDIAN);
        if (subtype == 0x2)
          proto_tree_add_item(fixed, hf_cur_ap, tvb, off + 4, 6, ENC_NA);
        break;
      case 0x1:
      case 0x3:
        add_cap(fixed, tvb, off);
        proto_tree_add_item(fixed, hf_status, tvb, off + 2, 2,
                            ENC_LITTLE_ENDIAN);
        proto_tree_add_item(fixed, hf_aid, tvb, off + 4, 2,
                            ENC_LITTLE_ENDIAN);
        break;
      case 0x5:
      case 0x8:
        proto_tree_add_item(fixed, hf_ts, tvb, off, 8, ENC_LITTLE_ENDIAN);
        proto_tree_add_item(fixed, hf_bi, tvb, off + 8, 2, ENC_LITTLE_ENDIAN);
        add_cap(fixed, tvb, off + 10);
        col_append_fstr(pinfo->cinfo, COL_INFO, ", BI=%u",
                        tvb_get_letohs(tvb, off + 8));
        break;
      case 0xa:
      case 0xc:
        proto_tree_add_item(fixed, hf_reason, tvb, off, 2, ENC_LITTLE_ENDIAN);
        col_append_fstr(pinfo->cinfo, COL_INFO, ", %s",
          val_to_str_const(tvb_get_letohs(tvb, off), reason_vals, "Reason"));
        break;
      case 0xb:
        proto_tree_add_item(fixed, hf_auth_alg, tvb, off, 2,
                            ENC_LITTLE_ENDIAN);
        proto_tree_add_item(fixed, hf_auth_seq, tvb, off + 2, 2,
                            ENC_LITTLE_ENDIAN);
        proto_tree_add_item(fixed, hf_status, tvb, off + 4, 2,
                            ENC_LITTLE_ENDIAN);
        break;
      case 0xd:
      case 0xe:
        proto_tree_add_item(fixed, hf_action_cat, tvb, off, 1, ENC_NA);
        col_append_fstr(pinfo->cinfo, COL_INFO, ", %s",
          val_to_str_const(tvb_get_uint8(tvb, off), action_cat_vals,
                           "Category"));
        break;
      default:
        break;
      }
    off += flen;
    }
  if ((subtype == 0xd) || (subtype == 0xe))
    {
    if (off < end)
      proto_tree_add_item(tree, hf_tag_data, tvb, off, end - off, ENC_NA);
    return;
    }
  add_tagged(tvb, pinfo, tree, off, end);
}

static int dissect_wlan(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                        void *data)
{
  proto_item *ti;
  proto_tree *wlan_tree, *fc_tree, *qos_tree;
  static int * const flag_bits[] = {
    &hf_fc_tods, &hf_fc_fromds, &hf_fc_frag, &hf_fc_retry, &hf_fc_pwrmgt,
    &hf_fc_moredata, &hf_fc_protected, &hf_fc_order, NULL };
  int has_fcs = GPOINTER_TO_INT(data);
  int len = tvb_captured_length(tvb), end, hdr_len, off;
  uint8_t fc0, flags;
  int type, subtype, type_subtype, ds, has_a2, has_a3;
  int ra, ta, da = -1, sa = -1, bssid = -1;
  uint16_t seq;
  static char flagstr[9];
  int i;

  col_set_str(pinfo->cinfo, COL_PROTOCOL, "802.11");
  if (len < 10)
    {
    col_set_str(pinfo->cinfo, COL_INFO, "802.11 frame too short");
    call_dissector(data_handle, tvb, pinfo, tree);
    return len;
    }
  end = has_fcs ? len - 4 : len;
  fc0 = tvb_get_uint8(tvb, 0);
  flags = tvb_get_uint8(tvb, 1);
  type = (fc0 >> 2) & 0x3;
  subtype = (fc0 >> 4) & 0xf;
  type_subtype = (type << 4) | subtype;
  ds = flags & (FLAG_TO_DS | FLAG_FROM_DS);

  /* the length of the header, by type */
  has_a2 = has_a3 = 0;
  if (type == FC_TYPE_CTRL)
    {
    has_a2 = !((subtype == 0xc) || (subtype == 0xd));
    hdr_len = has_a2 ? 16 : 10;
    }
  else
    {
    has_a2 = has_a3 = 1;
    hdr_len = 24;
    if ((type == FC_TYPE_DATA) && (ds == (FLAG_TO_DS | FLAG_FROM_DS)))
      hdr_len += 6;
    if ((type == FC_TYPE_DATA) && (subtype & 0x8))
      hdr_len += 2;
    if ((flags & FLAG_ORDER) &&
        ((type == FC_TYPE_MGT) || ((type == FC_TYPE_DATA) && (subtype & 0x8))))
      hdr_len += 4;
    }
  if (hdr_len > end)
    hdr_len = end;

  for (i = 0; i < 8; i++)
    flagstr[i] = (flags & (1 << (7 - i))) ? "OpPMRFTC"[i] : '.';
  flagstr[8] = 0;
  col_add_str(pinfo->cinfo, COL_INFO,
              val_to_str_const(type_subtype, type_subtype_vals,
                               "Unrecognized (Reserved frame)"));

  ti = proto_tree_add_protocol_format(tree, proto_wlan, tvb, 0, hdr_len,
         "IEEE 802.11 %s, Flags: %s",
         val_to_str_const(type_subtype, type_subtype_vals, "frame"), flagstr);
  wlan_tree = proto_item_add_subtree(ti, ett_wlan);
  proto_tree_add_uint(wlan_tree, hf_fc_type_subtype, tvb, 0, 1, type_subtype);
  ti = proto_tree_add_item(wlan_tree, hf_fc, tvb, 0, 2, ENC_BIG_ENDIAN);
  fc_tree = proto_item_add_subtree(ti, ett_fc);
  proto_tree_add_uint(fc_tree, hf_fc_type, tvb, 0, 1, type);
  proto_tree_add_uint(fc_tree, hf_fc_subtype, tvb, 0, 1, subtype);
  proto_tree_add_bitmask(fc_tree, tvb, 1, hf_fc_flags, ett_flags, flag_bits,
                         ENC_NA);
  proto_tree_add_item(wlan_tree, hf_duration, tvb, 2, 2, ENC_LITTLE_ENDIAN);

  /* which address is which, by the two ds bits */
  ra = 4;
  ta = has_a2 ? 10 : -1;
  if (type != FC_TYPE_CTRL)
    {
    switch (ds)
      {
      case 0:            da = 4;  sa = 10; bssid = 16; break;
      case FLAG_TO_DS:   bssid = 4; sa = 10; da = 16;  break;
      case FLAG_FROM_DS: da = 4;  bssid = 10; sa = 16; break;
      default:           da = 16; sa = (hdr_len >= 30) ? 24 : -1; break;
      }
    }
  proto_tree_add_item(wlan_tree, hf_ra, tvb, ra, 6, ENC_NA);
  proto_tree_add_item(wlan_tree, hf_addr, tvb, ra, 6, ENC_NA);
  if ((ta > 0) && (ta + 6 <= hdr_len))
    {
    proto_tree_add_item(wlan_tree, hf_ta, tvb, ta, 6, ENC_NA);
    proto_tree_add_item(wlan_tree, hf_addr, tvb, ta, 6, ENC_NA);
    }
  if ((da > 0) && (da + 6 <= hdr_len))
    proto_tree_add_item(wlan_tree, hf_da, tvb, da, 6, ENC_NA);
  if ((sa > 0) && (sa + 6 <= hdr_len))
    proto_tree_add_item(wlan_tree, hf_sa, tvb, sa, 6, ENC_NA);
  if ((bssid > 0) && (bssid + 6 <= hdr_len))
    proto_tree_add_item(wlan_tree, hf_bssid, tvb, bssid, 6, ENC_NA);

  /* source and destination columns: sa and da, else ta and ra */
  if ((sa > 0) && (sa + 6 <= hdr_len))
    set_address_tvb(&pinfo->dl_src, AT_ETHER, 6, tvb, sa);
  else if ((ta > 0) && (ta + 6 <= hdr_len))
    set_address_tvb(&pinfo->dl_src, AT_ETHER, 6, tvb, ta);
  if ((da > 0) && (da + 6 <= hdr_len))
    set_address_tvb(&pinfo->dl_dst, AT_ETHER, 6, tvb, da);
  else
    set_address_tvb(&pinfo->dl_dst, AT_ETHER, 6, tvb, ra);
  copy_address_shallow(&pinfo->src, &pinfo->dl_src);
  copy_address_shallow(&pinfo->dst, &pinfo->dl_dst);

  if (type != FC_TYPE_CTRL)
    {
    seq = tvb_get_letohs(tvb, 22);
    proto_tree_add_item(wlan_tree, hf_frag, tvb, 22, 2, ENC_LITTLE_ENDIAN);
    proto_tree_add_item(wlan_tree, hf_seq, tvb, 22, 2, ENC_LITTLE_ENDIAN);
    col_append_fstr(pinfo->cinfo, COL_INFO, ", SN=%u, FN=%u, Flags=%s",
                    seq >> 4, seq & 0xf, flagstr);
    if ((type == FC_TYPE_DATA) && (subtype & 0x8))
      {
      off = (ds == (FLAG_TO_DS | FLAG_FROM_DS)) ? 30 : 24;
      if (off + 2 <= hdr_len)
        {
        ti = proto_tree_add_item(wlan_tree, hf_qos, tvb, off, 2,
                                 ENC_LITTLE_ENDIAN);
        qos_tree = proto_item_add_subtree(ti, ett_qos);
        proto_tree_add_item(qos_tree, hf_qos_tid, tvb, off, 2,
                            ENC_LITTLE_ENDIAN);
        }
      }
    }
  else
    col_append_fstr(pinfo->cinfo, COL_INFO, ", Flags=%s", flagstr);

  if (has_fcs && (len >= 4))
    proto_tree_add_item(wlan_tree, hf_fcs, tvb, len - 4, 4,
                        ENC_LITTLE_ENDIAN);

  off = hdr_len;
  if (off >= end)
    return len;
  if (type == FC_TYPE_MGT)
    {
    ti = proto_tree_add_protocol_format(tree, proto_wlan, tvb, off,
                                        end - off,
                                        "IEEE 802.11 Wireless Management");
    dissect_mgt_body(tvb, pinfo, proto_item_add_subtree(ti, ett_fixed),
                     subtype, off, end);
    }
  else if (type == FC_TYPE_DATA)
    {
    if (subtype & 0x4)
      return len;
    if (flags & FLAG_PROTECTED)
      {
      col_append_str(pinfo->cinfo, COL_INFO, " (encrypted)");
      if (off + 8 <= end)
        {
        uint64_t pn = tvb_get_uint8(tvb, off) |
          ((uint64_t) tvb_get_uint8(tvb, off + 1) << 8) |
          ((uint64_t) tvb_get_uint8(tvb, off + 4) << 16) |
          ((uint64_t) tvb_get_uint8(tvb, off + 5) << 24) |
          ((uint64_t) tvb_get_uint8(tvb, off + 6) << 32) |
          ((uint64_t) tvb_get_uint8(tvb, off + 7) << 40);
        proto_tree_add_uint64(wlan_tree, hf_ccmp_pn, tvb, off, 8, pn);
        off += 8;
        }
      if (off < end)
        proto_tree_add_item(tree, hf_wep_data, tvb, off, end - off, ENC_NA);
      return len;
    }
    if ((off + 8 <= end) && (tvb_get_uint8(tvb, off) == 0xaa) &&
        (tvb_get_uint8(tvb, off + 1) == 0xaa) &&
        (tvb_get_uint8(tvb, off + 2) == 0x03))
      add_llc(tvb_new_subset_length(tvb, 0, end), pinfo, tree, wlan_tree,
              off);
    else
      call_dissector(data_handle, tvb_new_subset_length(tvb, off, end - off),
                     pinfo, tree);
    }
  else if (off < end)
    call_dissector(data_handle, tvb_new_subset_length(tvb, off, end - off),
                   pinfo, tree);
  return len;
}

/* EAPOL, and the 4-way handshake message the key frame is */
static int dissect_eapol(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                         void *data)
{
  proto_item *ti;
  proto_tree *et;
  int len = tvb_captured_length(tvb), type, msg = 0;
  uint16_t info;
  col_set_str(pinfo->cinfo, COL_PROTOCOL, "EAPOL");
  if (len < 4)
    return 0;
  type = tvb_get_uint8(tvb, 1);
  ti = proto_tree_add_item(tree, proto_eapol, tvb, 0, len, ENC_NA);
  et = proto_item_add_subtree(ti, ett_eapol);
  proto_tree_add_item(et, hf_eapol_version, tvb, 0, 1, ENC_NA);
  proto_tree_add_item(et, hf_eapol_type, tvb, 1, 1, ENC_NA);
  proto_tree_add_item(et, hf_eapol_len, tvb, 2, 2, ENC_BIG_ENDIAN);
  col_add_str(pinfo->cinfo, COL_INFO,
              val_to_str_const(type, eapol_type_vals, "Unknown"));
  if ((type != 3) || (len < 99))
    return len;
  info = tvb_get_ntohs(tvb, 5);
  proto_tree_add_item(et, hf_eapol_desc, tvb, 4, 1, ENC_NA);
  proto_tree_add_item(et, hf_eapol_keyinfo, tvb, 5, 2, ENC_BIG_ENDIAN);
  proto_tree_add_item(et, hf_eapol_keylen, tvb, 7, 2, ENC_BIG_ENDIAN);
  proto_tree_add_item(et, hf_eapol_replay, tvb, 9, 8, ENC_BIG_ENDIAN);
  proto_tree_add_item(et, hf_eapol_nonce, tvb, 17, 32, ENC_NA);
  proto_tree_add_item(et, hf_eapol_mic, tvb, 81, 16, ENC_NA);
  proto_tree_add_item(et, hf_eapol_keydata_len, tvb, 97, 2, ENC_BIG_ENDIAN);
  /* pairwise: ack alone 1, mic alone 2 or 4, ack and mic and install 3 */
  if (info & 0x0008)
    {
    if ((info & 0x0080) && !(info & 0x0100))
      msg = 1;
    else if ((info & 0x0080) && (info & 0x0100))
      msg = 3;
    else if (info & 0x0100)
      msg = (tvb_get_ntohs(tvb, 97) == 0) ? 4 : 2;
    }
  if (msg)
    {
    col_append_fstr(pinfo->cinfo, COL_INFO, " (Message %d of 4)", msg);
    proto_item_append_text(ti, ", Message %d of 4", msg);
    }
  else
    col_append_str(pinfo->cinfo, COL_INFO, " (Group Message)");
  return len;
}

void proto_register_wlan(void)
{
  static hf_register_info hf_rt[] = {
    { &hf_rt_version, { "Header revision", "radiotap.version", FT_UINT8,
      BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_rt_len, { "Header length", "radiotap.length", FT_UINT16,
      BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_rt_present, { "Present flags", "radiotap.present.word",
      FT_UINT32, BASE_HEX, NULL, 0x0, NULL, HFILL }},
    { &hf_rt_tsft, { "MAC timestamp", "radiotap.mactime", FT_UINT64,
      BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_rt_flags, { "Flags", "radiotap.flags", FT_UINT8, BASE_HEX, NULL,
      0x0, NULL, HFILL }},
    { &hf_rt_fcs_flag, { "FCS at end", "radiotap.flags.fcs", FT_BOOLEAN, 8,
      NULL, RT_FLAG_FCS, NULL, HFILL }},
    { &hf_rt_rate, { "Data rate", "radiotap.datarate", FT_UINT8, BASE_DEC,
      NULL, 0x0, NULL, HFILL }},
    { &hf_rt_freq, { "Channel frequency", "radiotap.channel.freq",
      FT_UINT16, BASE_DEC|BASE_UNIT_STRING, UNS(&units_mhz), 0x0, NULL,
      HFILL }},
    { &hf_rt_chflags, { "Channel flags", "radiotap.channel.flags",
      FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL }},
    { &hf_rt_dbm_signal, { "Antenna signal", "radiotap.dbm_antsignal",
      FT_INT8, BASE_DEC|BASE_UNIT_STRING, UNS(&units_dbm), 0x0, NULL,
      HFILL }},
    { &hf_rt_dbm_noise, { "Antenna noise", "radiotap.dbm_antnoise",
      FT_INT8, BASE_DEC|BASE_UNIT_STRING, UNS(&units_dbm), 0x0, NULL,
      HFILL }},
    { &hf_rt_antenna, { "Antenna", "radiotap.antenna", FT_UINT8, BASE_DEC,
      NULL, 0x0, NULL, HFILL }},
  };
  static hf_register_info hf_wlan[] = {
    { &hf_fc_type_subtype, { "Type/Subtype", "wlan.fc.type_subtype",
      FT_UINT8, BASE_HEX, VALS(type_subtype_vals), 0x0, NULL, HFILL }},
    { &hf_fc, { "Frame Control Field", "wlan.fc", FT_UINT16, BASE_HEX,
      NULL, 0x0, NULL, HFILL }},
    { &hf_fc_type, { "Type", "wlan.fc.type", FT_UINT8, BASE_DEC,
      VALS(type_vals), 0x0, NULL, HFILL }},
    { &hf_fc_subtype, { "Subtype", "wlan.fc.subtype", FT_UINT8, BASE_DEC,
      NULL, 0x0, NULL, HFILL }},
    { &hf_fc_flags, { "Flags", "wlan.flags", FT_UINT8, BASE_HEX, NULL, 0x0,
      NULL, HFILL }},
    { &hf_fc_tods, { "To DS", "wlan.fc.tods", FT_BOOLEAN, 8, NULL, 0x01,
      NULL, HFILL }},
    { &hf_fc_fromds, { "From DS", "wlan.fc.fromds", FT_BOOLEAN, 8, NULL,
      0x02, NULL, HFILL }},
    { &hf_fc_frag, { "More Fragments", "wlan.fc.frag", FT_BOOLEAN, 8, NULL,
      0x04, NULL, HFILL }},
    { &hf_fc_retry, { "Retry", "wlan.fc.retry", FT_BOOLEAN, 8, NULL, 0x08,
      NULL, HFILL }},
    { &hf_fc_pwrmgt, { "PWR MGT", "wlan.fc.pwrmgt", FT_BOOLEAN, 8, NULL,
      0x10, NULL, HFILL }},
    { &hf_fc_moredata, { "More Data", "wlan.fc.moredata", FT_BOOLEAN, 8,
      NULL, 0x20, NULL, HFILL }},
    { &hf_fc_protected, { "Protected flag", "wlan.fc.protected",
      FT_BOOLEAN, 8, NULL, 0x40, NULL, HFILL }},
    { &hf_fc_order, { "+HTC/Order flag", "wlan.fc.order", FT_BOOLEAN, 8,
      NULL, 0x80, NULL, HFILL }},
    { &hf_duration, { "Duration", "wlan.duration", FT_UINT16, BASE_DEC,
      NULL, 0x7fff, NULL, HFILL }},
    { &hf_ra, { "Receiver address", "wlan.ra", FT_ETHER, BASE_NONE, NULL,
      0x0, NULL, HFILL }},
    { &hf_ta, { "Transmitter address", "wlan.ta", FT_ETHER, BASE_NONE,
      NULL, 0x0, NULL, HFILL }},
    { &hf_da, { "Destination address", "wlan.da", FT_ETHER, BASE_NONE,
      NULL, 0x0, NULL, HFILL }},
    { &hf_sa, { "Source address", "wlan.sa", FT_ETHER, BASE_NONE, NULL,
      0x0, NULL, HFILL }},
    { &hf_bssid, { "BSS Id", "wlan.bssid", FT_ETHER, BASE_NONE, NULL, 0x0,
      NULL, HFILL }},
    { &hf_addr, { "Hardware address", "wlan.addr", FT_ETHER, BASE_NONE,
      NULL, 0x0, NULL, HFILL }},
    { &hf_frag, { "Fragment number", "wlan.frag", FT_UINT16, BASE_DEC,
      NULL, 0x000f, NULL, HFILL }},
    { &hf_seq, { "Sequence number", "wlan.seq", FT_UINT16, BASE_DEC, NULL,
      0xfff0, NULL, HFILL }},
    { &hf_qos, { "QoS Control", "wlan.qos", FT_UINT16, BASE_HEX, NULL, 0x0,
      NULL, HFILL }},
    { &hf_qos_tid, { "TID", "wlan.qos.tid", FT_UINT16, BASE_DEC, NULL,
      0x000f, NULL, HFILL }},
    { &hf_fcs, { "Frame check sequence", "wlan.fcs", FT_UINT32, BASE_HEX,
      NULL, 0x0, NULL, HFILL }},
    { &hf_ts, { "Timestamp", "wlan.fixed.timestamp", FT_UINT64, BASE_DEC,
      NULL, 0x0, NULL, HFILL }},
    { &hf_bi, { "Beacon Interval", "wlan.fixed.beacon", FT_UINT16,
      BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_cap, { "Capabilities Information", "wlan.fixed.capabilities",
      FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL }},
    { &hf_cap_ess, { "ESS capabilities", "wlan.fixed.capabilities.ess",
      FT_BOOLEAN, 16, NULL, 0x0001, NULL, HFILL }},
    { &hf_cap_ibss, { "IBSS status", "wlan.fixed.capabilities.ibss",
      FT_BOOLEAN, 16, NULL, 0x0002, NULL, HFILL }},
    { &hf_cap_privacy, { "Privacy", "wlan.fixed.capabilities.privacy",
      FT_BOOLEAN, 16, NULL, 0x0010, NULL, HFILL }},
    { &hf_cap_short_preamble, { "Short Preamble",
      "wlan.fixed.capabilities.preamble", FT_BOOLEAN, 16, NULL, 0x0020,
      NULL, HFILL }},
    { &hf_cap_short_slot, { "Short Slot Time",
      "wlan.fixed.capabilities.short_slot_time", FT_BOOLEAN, 16, NULL,
      0x0400, NULL, HFILL }},
    { &hf_listen, { "Listen Interval", "wlan.fixed.listen_ival", FT_UINT16,
      BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_status, { "Status code", "wlan.fixed.status_code", FT_UINT16,
      BASE_DEC, VALS(status_vals), 0x0, NULL, HFILL }},
    { &hf_aid, { "Association ID", "wlan.fixed.aid", FT_UINT16, BASE_DEC,
      NULL, 0x3fff, NULL, HFILL }},
    { &hf_cur_ap, { "Current AP", "wlan.fixed.current_ap", FT_ETHER,
      BASE_NONE, NULL, 0x0, NULL, HFILL }},
    { &hf_auth_alg, { "Authentication Algorithm", "wlan.fixed.auth.alg",
      FT_UINT16, BASE_DEC, VALS(auth_alg_vals), 0x0, NULL, HFILL }},
    { &hf_auth_seq, { "Authentication SEQ", "wlan.fixed.auth_seq",
      FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_reason, { "Reason code", "wlan.fixed.reason_code", FT_UINT16,
      BASE_DEC, VALS(reason_vals), 0x0, NULL, HFILL }},
    { &hf_action_cat, { "Category code", "wlan.fixed.category_code",
      FT_UINT8, BASE_DEC, VALS(action_cat_vals), 0x0, NULL, HFILL }},
    { &hf_tag_number, { "Tag Number", "wlan.tag.number", FT_UINT8,
      BASE_DEC, VALS(tag_vals), 0x0, NULL, HFILL }},
    { &hf_tag_length, { "Tag length", "wlan.tag.length", FT_UINT8,
      BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_ssid, { "SSID", "wlan.ssid", FT_STRING, BASE_NONE, NULL, 0x0,
      NULL, HFILL }},
    { &hf_rate_mbps, { "Supported Rates", "wlan.supported_rates",
      FT_UINT8, BASE_HEX, NULL, 0x0, NULL, HFILL }},
    { &hf_ds_channel, { "Current Channel", "wlan.ds.current_channel",
      FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_tim_dtim_count, { "DTIM count", "wlan.tim.dtim_count", FT_UINT8,
      BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_tim_dtim_period, { "DTIM period", "wlan.tim.dtim_period",
      FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_country, { "Code", "wlan.country_info.code", FT_STRING,
      BASE_NONE, NULL, 0x0, NULL, HFILL }},
    { &hf_rsn_version, { "RSN Version", "wlan.rsn.version", FT_UINT16,
      BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_rsn_gcs, { "Group Cipher Suite type", "wlan.rsn.gcs.type",
      FT_UINT8, BASE_DEC, VALS(rsn_cipher_vals), 0x0, NULL, HFILL }},
    { &hf_rsn_pcs, { "Pairwise Cipher Suite type", "wlan.rsn.pcs.type",
      FT_UINT8, BASE_DEC, VALS(rsn_cipher_vals), 0x0, NULL, HFILL }},
    { &hf_rsn_akms, { "Auth Key Management (AKM) type", "wlan.rsn.akms.type",
      FT_UINT8, BASE_DEC, VALS(rsn_akm_vals), 0x0, NULL, HFILL }},
    { &hf_rsn_cap, { "RSN Capabilities", "wlan.rsn.capabilities",
      FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL }},
    { &hf_ht_primary, { "Primary Channel", "wlan.ht.info.primarychannel",
      FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_vendor_oui, { "OUI", "wlan.tag.oui", FT_UINT24, BASE_HEX, NULL,
      0x0, NULL, HFILL }},
    { &hf_vendor_type, { "Vendor Specific OUI Type", "wlan.tag.vendor.oui.type",
      FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_tag_data, { "Data", "wlan.tag.data", FT_BYTES, BASE_NONE, NULL,
      0x0, NULL, HFILL }},
    { &hf_ccmp_pn, { "CCMP PN", "wlan.ccmp.extiv", FT_UINT64, BASE_DEC,
      NULL, 0x0, NULL, HFILL }},
    { &hf_wep_data, { "Encrypted data", "wlan.encrypted_data", FT_BYTES,
      BASE_NONE, NULL, 0x0, NULL, HFILL }},
    { &hf_llc_dsap, { "DSAP", "llc.dsap", FT_UINT8, BASE_HEX, NULL, 0x0,
      NULL, HFILL }},
    { &hf_llc_ssap, { "SSAP", "llc.ssap", FT_UINT8, BASE_HEX, NULL, 0x0,
      NULL, HFILL }},
    { &hf_llc_ctrl, { "Control field", "llc.control", FT_UINT8, BASE_HEX,
      NULL, 0x0, NULL, HFILL }},
    { &hf_llc_oui, { "Organization Code", "llc.oui", FT_UINT24, BASE_HEX,
      NULL, 0x0, NULL, HFILL }},
    { &hf_llc_etype, { "Type", "llc.type", FT_UINT16, BASE_HEX, NULL, 0x0,
      NULL, HFILL }},
  };
  static hf_register_info hf_eapol[] = {
    { &hf_eapol_version, { "Version", "eapol.version", FT_UINT8, BASE_DEC,
      NULL, 0x0, NULL, HFILL }},
    { &hf_eapol_type, { "Type", "eapol.type", FT_UINT8, BASE_DEC,
      VALS(eapol_type_vals), 0x0, NULL, HFILL }},
    { &hf_eapol_len, { "Length", "eapol.len", FT_UINT16, BASE_DEC, NULL,
      0x0, NULL, HFILL }},
    { &hf_eapol_desc, { "Key Descriptor Type", "eapol.keydes.type",
      FT_UINT8, BASE_DEC, VALS(eapol_desc_vals), 0x0, NULL, HFILL }},
    { &hf_eapol_keyinfo, { "Key Information", "wlan_rsna_eapol.keydes.key_info",
      FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL }},
    { &hf_eapol_keylen, { "Key Length", "eapol.keydes.key_len", FT_UINT16,
      BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_eapol_replay, { "Replay Counter", "eapol.keydes.replay_counter",
      FT_UINT64, BASE_DEC, NULL, 0x0, NULL, HFILL }},
    { &hf_eapol_nonce, { "WPA Key Nonce", "wlan_rsna_eapol.keydes.nonce",
      FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL }},
    { &hf_eapol_mic, { "WPA Key MIC", "wlan_rsna_eapol.keydes.mic",
      FT_BYTES, BASE_NONE, NULL, 0x0, NULL, HFILL }},
    { &hf_eapol_keydata_len, { "WPA Key Data Length",
      "wlan_rsna_eapol.keydes.data_len", FT_UINT16, BASE_DEC, NULL, 0x0,
      NULL, HFILL }},
  };
  static int *ett[] = {
    &ett_rt, &ett_wlan, &ett_fc, &ett_flags, &ett_fixed, &ett_tagged,
    &ett_tag, &ett_cap, &ett_qos, &ett_llc, &ett_eapol,
  };

  proto_radiotap = proto_register_protocol("IEEE 802.11 Radiotap Capture header",
                                           "Radiotap", "radiotap");
  proto_wlan = proto_register_protocol("IEEE 802.11 wireless LAN",
                                       "IEEE 802.11", "wlan");
  proto_eapol = proto_register_protocol("802.1X Authentication", "EAPOL",
                                        "eapol");
  proto_register_field_array(proto_radiotap, hf_rt, array_length(hf_rt));
  proto_register_field_array(proto_wlan, hf_wlan, array_length(hf_wlan));
  proto_register_field_array(proto_eapol, hf_eapol, array_length(hf_eapol));
  proto_register_subtree_array(ett, array_length(ett));
  radiotap_handle = register_dissector("radiotap", dissect_radiotap,
                                       proto_radiotap);
  wlan_handle = register_dissector("wlan", dissect_wlan, proto_wlan);
  eapol_handle = register_dissector("eapol", dissect_eapol, proto_eapol);
}

void proto_reg_handoff_wlan(void)
{
  ethertype_handle = find_dissector_add_dependency("ethertype", proto_wlan);
  data_handle = find_dissector("data");
  dissector_add_uint("wtap_encap", WTAP_ENCAP_IEEE_802_11_RADIOTAP,
                     radiotap_handle);
  dissector_add_uint("wtap_encap", WTAP_ENCAP_IEEE_802_11, wlan_handle);
  dissector_add_uint("ethertype", ETHERTYPE_EAPOL, eapol_handle);
}
