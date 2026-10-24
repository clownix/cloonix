# Makefile for webshark — Wireshark engine with browser frontend
# Pure C, no CMake, no C++, no Qt.
#
# Usage:
#   make              build ./build/bin/cloonix-webshark
#   make clean
#   make info         show compiler config
#
# Runtime (from any directory):
#   /path/to/cloonix_gui_server /var/lib/cloonix/nemo/snf 4444
#   then open http://localhost:4444/
#
# SPDX-License-Identifier: GPL-2.0-or-later

# ── Toolchain ─────────────────────────────────────────────────────────────────
CC := gcc

# ── Build directories ──────────────────────────────────────────────────────────
B := build
BINDIR := $(B)/bin

# ── pkg-config ────────────────────────────────────────────────────────────────
GLIB_CFLAGS  := $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS    := $(shell pkg-config --libs   glib-2.0 gthread-2.0)
PCRE2_CFLAGS := $(shell pkg-config --cflags libpcre2-8)
PCRE2_LIBS   := $(shell pkg-config --libs   libpcre2-8)
ZLIB_LIBS    := $(shell pkg-config --libs   zlib 2>/dev/null || echo -lz)
PCAP_CFLAGS  := $(shell pkg-config --cflags libpcap 2>/dev/null)
PCAP_LIBS    := $(shell pkg-config --libs   libpcap 2>/dev/null)

# ── Compiler flags ─────────────────────────────────────────────────────────────
CFLAGS := \
	-std=c11 -O2 -g -ffunction-sections -fdata-sections \
	-Wall -Wextra \
	-Wno-unused-parameter \
	-Wno-format-nonliteral \
	-Wno-format-truncation \
	-Wno-pointer-sign \
	-Wno-unused-function \
	-Wno-unused-variable \
	-Wno-unused-but-set-variable \
	-Wno-unused-const-variable \
	-Wno-unused-label \
	-Wno-sign-compare \
	-fPIC \
	-D_GNU_SOURCE \
	-DHAVE_CONFIG_H \
	-I. \
	-Iinclude \
	-I$(B) \
	-Iwsutil \
	-Iwiretap \
	-Iui \
	$(GLIB_CFLAGS) \
	$(PCRE2_CFLAGS) \
	$(PCAP_CFLAGS)

# the linker drops what no binary reaches
LDGC ?= -Wl,--gc-sections

LIBS := \
	$(GLIB_LIBS) \
	$(PCRE2_LIBS) \
	$(ZLIB_LIBS) \
	$(PCAP_LIBS) \
	-lpthread -lm -ldl

# ── Source lists ───────────────────────────────────────────────────────────────

# Sources shared between webshark and dumpcap
WSUTIL_C := \
	wsutil/wmem/wmem_array.c \
	wsutil/wmem/wmem_core.c \
	wsutil/wmem/wmem_allocator_block.c \
	wsutil/wmem/wmem_allocator_block_fast.c \
	wsutil/wmem/wmem_allocator_simple.c \
	wsutil/wmem/wmem_allocator_strict.c \
	wsutil/wmem/wmem_interval_tree.c \
	wsutil/wmem/wmem_list.c \
	wsutil/wmem/wmem_map.c \
	wsutil/wmem/wmem_miscutl.c \
	wsutil/wmem/wmem_strbuf.c \
	wsutil/wmem/wmem_strutl.c \
	wsutil/wmem/wmem_tree.c \
	wsutil/wmem/wmem_user_cb.c \
	wsutil/buffer.c \
	wsutil/cmdarg_err.c \
	wsutil/cpu_info.c \
	wsutil/crash_info.c \
	wsutil/crc32.c \
	wsutil/inet_addr.c \
	wsutil/inet_cidr.c \
	wsutil/nstime.c \
	wsutil/os_version_info.c \
	wsutil/privileges.c \
	wsutil/regex.c \
	wsutil/report_message.c \
	wsutil/str_util.c \
	wsutil/strtoi.c \
	wsutil/tempfile.c \
	wsutil/time_util.c \
	wsutil/to_str.c \
	wsutil/unicode-utils.c \
	wsutil/version_info.c \
	wsutil/ws_getopt.c \
	wsutil/ws_mempbrk.c \
	wsutil/ws_strptime.c

WIRETAP_C := \
	wiretap/erf.c \
	wiretap/file_access.c \
	wiretap/file_wrappers.c \
	wiretap/libpcap.c \
	wiretap/pcap-common.c \
	wiretap/pcapng.c \
	wiretap/wtap.c \
	wiretap/wtap_opttypes.c

EPAN_C := \
	epan/address_types.c \
	epan/afn.c \
	epan/capture_dissectors.c \
	epan/charsets.c \
	epan/color_filters.c \
	epan/column-utils.c \
	epan/column.c \
	epan/conversation.c \
	epan/conversation_filter.c \
	epan/conversation_table.c \
	epan/crc32-tvb.c \
	epan/decode_as.c \
	epan/disabled_protos.c \
	epan/epan.c \
	epan/except.c \
	epan/expert.c \
	epan/exported_pdu.c \
	epan/filter_expressions.c \
	epan/follow.c \
	epan/frame_data.c \
	epan/frame_data_sequence.c \
	epan/grammar.c \
	epan/guid-utils.c \
	epan/in_cksum.c \
	epan/ipproto.c \
	epan/oids.c \
	epan/osi-utils.c \
	epan/packet.c \
	epan/prefs.c \
	epan/proto.c \
	epan/proto_data.c \
	epan/range.c \
	epan/reassemble.c \
	epan/register.c \
	epan/scanner.c \
	epan/sequence_analysis.c \
	epan/show_exception.c \
	epan/stat_tap_ui.c \
	epan/strutil.c \
	epan/tap.c \
	epan/tfs.c \
	epan/timestamp.c \
	epan/to_str.c \
	epan/tvbuff.c \
	epan/tvbuff_base64.c \
	epan/tvbuff_composite.c \
	epan/tvbuff_real.c \
	epan/tvbuff_subset.c \
	epan/uat.c \
	epan/uat_load.c \
	epan/unit_strings.c \
	epan/value_string.c \
	epan/wmem_scopes.c \
	epan/dfilter/dfilter.c \
	epan/dfilter/dfunctions.c \
	epan/dfilter/dfvm.c \
	epan/dfilter/drange.c \
	epan/dfilter/gencode.c \
	epan/dfilter/semcheck.c \
	epan/dfilter/sttype-field.c \
	epan/dfilter/sttype-function.c \
	epan/dfilter/sttype-number.c \
	epan/dfilter/sttype-op.c \
	epan/dfilter/sttype-pointer.c \
	epan/dfilter/sttype-set.c \
	epan/dfilter/sttype-slice.c \
	epan/dfilter/sttype-string.c \
	epan/dfilter/syntax-tree.c \
	epan/ftypes/ftype-bytes.c \
	epan/ftypes/ftype-double.c \
	epan/ftypes/ftype-guid.c \
	epan/ftypes/ftype-ieee-11073-float.c \
	epan/ftypes/ftype-integer.c \
	epan/ftypes/ftype-ipv4.c \
	epan/ftypes/ftype-ipv6.c \
	epan/ftypes/ftype-none.c \
	epan/ftypes/ftype-protocol.c \
	epan/ftypes/ftype-string.c \
	epan/ftypes/ftype-time.c \
	epan/ftypes/ftypes.c \
	epan/dissectors/dissectors.c \
	epan/dissectors/packet-arp.c \
	epan/dissectors/packet-data.c \
	epan/dissectors/packet-dhcp.c \
	epan/dissectors/packet-dns.c \
	epan/dissectors/packet-eth.c \
	epan/dissectors/packet-ethertype.c \
	epan/dissectors/packet-frame.c \
	epan/dissectors/packet-gre.c \
	epan/dissectors/packet-icmp.c \
	epan/dissectors/packet-ieee8023.c \
	epan/dissectors/packet-igmp.c \
	epan/dissectors/packet-ip.c \
	epan/dissectors/packet-media-type.c \
	epan/dissectors/packet-media.c \
	epan/dissectors/packet-smtp.c \
	epan/dissectors/packet-tcp.c \
	epan/dissectors/packet-text-media.c \
	epan/dissectors/packet-udp.c \
	epan/dissectors/packet-vlan.c \
	epan/dissectors/packet-wlan.c \

UI_C := \
	ui/alert_box.c \
	ui/simple_dialog_stubs.c

MAIN_C := \
	webshark.c \
	webshark_html.c \
	cfile.c \
	file_packet_provider.c \
	frame_tvbuff.c \
	sync_pipe_write.c \
	capture_opts.c

ALL_C := $(WSUTIL_C) $(WIRETAP_C) $(EPAN_C) $(UI_C) $(MAIN_C)

# ── dumpcap sources (shares wsutil + wiretap objects with webshark) ────────────
DUMPCAP_C := \
	dumpcap.c \
	cloonix_fanout.c \
	capture_opts.c \
	writecap/pcapio.c \
	ringbuffer.c \
	sync_pipe_write.c

DUMPCAP_OBJS := $(addprefix $(B)/,$(DUMPCAP_C:.c=.o))
SHARED_OBJS  := $(addprefix $(B)/,$(WSUTIL_C:.c=.o)) \
                $(addprefix $(B)/,$(WIRETAP_C:.c=.o))

# ── Object file derivation ─────────────────────────────────────────────────────
# Keep directory structure inside build/: foo/bar.c -> build/foo/bar.o
# This avoids name collisions (epan/to_str.c vs wsutil/to_str.c).

OBJS := $(addprefix $(B)/,$(ALL_C:.c=.o))

# ── Default target (must come first) ──────────────────────────────────────────
.PHONY: all clean info

all: $(BINDIR)/webshark $(BINDIR)/dumpcap

# ── Generated: embed webshark.html as a C byte array ──────────────────────────
HTML_INC := $(B)/webshark_html_data.inc

$(HTML_INC): webshark.html | $(B)
	@echo "  EMBED $<"
	@python3 -c "\
data = open('webshark.html','rb').read(); \
open('$@','w').write(','.join('0x{:02x}'.format(b) for b in data)+'\n')"

# webshark_html.o needs the generated include
$(B)/webshark_html.o: $(HTML_INC)

# ── Build targets ──────────────────────────────────────────────────────────────

$(BINDIR)/webshark: $(OBJS) | $(BINDIR)
	@echo "  LINK  $@"
	@$(CC) -o $@ $(OBJS) $(LIBS) $(LDGC) \
		-Wl,--wrap,capture_dissector_increment_count \
		-Wl,--wrap,try_capture_dissector \
		-Wl,--wrap,call_capture_dissector
	@echo ""
	@echo "Built: $@"
	@echo "Launch (from bin/): ./cloonix_gui_server /var/lib/cloonix/nemo/snf 4444"

$(BINDIR)/dumpcap: $(DUMPCAP_OBJS) $(SHARED_OBJS) | $(BINDIR)
	@echo "  LINK  $@"
	@$(CC) -o $@ $(DUMPCAP_OBJS) $(SHARED_OBJS) $(LIBS) $(LDGC)
	@echo "Built: $@"

# ── Compilation rules ──────────────────────────────────────────────────────────
# One pattern per source subtree, each with its own -I flags.
# Directory creation is handled by the | order-only prerequisite.

# wsutil (needs -Iwsutil/wmem implicitly via -Iwsutil)
$(B)/wsutil/%.o: wsutil/%.c | $(B)/wsutil $(B)/wsutil/wmem
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -DWS_BUILD_DLL -DBUILD_WSUTIL -Iwsutil/wmem -c -o $@ $<

# wiretap
$(B)/wiretap/%.o: wiretap/%.c | $(B)/wiretap
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

# epan top-level
$(B)/epan/%.o: epan/%.c | $(B)/epan
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -Iepan -c -o $@ $<

# epan/dfilter
$(B)/epan/dfilter/%.o: epan/dfilter/%.c | $(B)/epan/dfilter
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -Iepan -Iepan/dfilter -c -o $@ $<

# epan/ftypes
$(B)/epan/ftypes/%.o: epan/ftypes/%.c | $(B)/epan/ftypes
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -Iepan -Iepan/ftypes -c -o $@ $<

# epan/dissectors
$(B)/epan/dissectors/%.o: epan/dissectors/%.c | $(B)/epan/dissectors
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -Iepan -Iepan/dissectors -c -o $@ $<

# ui/
$(B)/ui/%.o: ui/%.c | $(B)/ui
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

# writecap/
$(B)/writecap/%.o: writecap/%.c | $(B)/writecap
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

# top-level .c files (webshark.c, cfile.c, file.c, ...)
$(B)/%.o: %.c | $(B)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

# ── Directory creation ─────────────────────────────────────────────────────────
BUILDDIRS := \
	$(B) \
	$(B)/wsutil \
	$(B)/wsutil/wmem \
	$(B)/wiretap \
	$(B)/epan \
	$(B)/epan/dfilter \
	$(B)/epan/ftypes \
	$(B)/epan/dissectors \
	$(B)/ui \
	$(B)/writecap \
	$(BINDIR)

$(BUILDDIRS):
	@mkdir -p $@

# ── Housekeeping ───────────────────────────────────────────────────────────────
clean:
	rm -rf $(B)

info:
	@echo "CC:          $(CC)"
	@echo "CFLAGS:      $(CFLAGS)"
	@echo "LIBS:        $(LIBS)"
	@echo "Source files: $(words $(ALL_C))"
	@echo "Object files: $(words $(OBJS))"
