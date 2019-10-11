/* Copyright (C) 2017 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "suricata-common.h"
#include "suricata.h"

#include "app-layer-protos.h"
#include "app-layer-detect-proto.h"
#include "app-layer-parser.h"
#include "app-layer-dns-common.h"

#include "util-unittest.h"

#include "app-layer-dns-tcp.h"
#include "rust-dns-dns-gen.h"

#ifdef UNITTESTS
static void RustDNSTCPParserRegisterTests(void);
#endif

static int RustDNSTCPParseRequest(Flow *f, void *state,
        AppLayerParserState *pstate, const uint8_t *input, uint32_t input_len,
        void *local_data, const uint8_t flags)
{
    SCLogDebug("RustDNSTCPParseRequest");
    return rs_dns_parse_request_tcp(f, state, pstate, input, input_len,
            local_data);
}

static int RustDNSTCPParseResponse(Flow *f, void *state,
        AppLayerParserState *pstate, const uint8_t *input, uint32_t input_len,
        void *local_data, const uint8_t flags)
{
    SCLogDebug("RustDNSTCPParseResponse");
    return rs_dns_parse_response_tcp(f, state, pstate, input, input_len,
            local_data);
}

static uint16_t RustDNSTCPProbe(Flow *f, uint8_t direction,
        const uint8_t *input, uint32_t len, uint8_t *rdir)
{
    SCLogDebug("RustDNSTCPProbe");
    if (len == 0 || len < sizeof(DNSHeader)) {
        return ALPROTO_UNKNOWN;
    }

    // Validate and return ALPROTO_FAILED if needed.
    if (!rs_dns_probe_tcp(direction, input, len, rdir)) {
        return ALPROTO_FAILED;
    }

    return ALPROTO_DNS;
}

static int RustDNSGetAlstateProgress(void *tx, uint8_t direction)
{
    return rs_dns_tx_get_alstate_progress(tx, direction);
}

static uint64_t RustDNSGetTxCnt(void *alstate)
{
    return rs_dns_state_get_tx_count(alstate);
}

static void *RustDNSGetTx(void *alstate, uint64_t tx_id)
{
    return rs_dns_state_get_tx(alstate, tx_id);
}

static void RustDNSSetTxLogged(void *alstate, void *tx, LoggerId logged)
{
    rs_dns_tx_set_logged(alstate, tx, logged);
}

static LoggerId RustDNSGetTxLogged(void *alstate, void *tx)
{
    return rs_dns_tx_get_logged(alstate, tx);
}

static void RustDNSStateTransactionFree(void *state, uint64_t tx_id)
{
    rs_dns_state_tx_free(state, tx_id);
}

static DetectEngineState *RustDNSGetTxDetectState(void *tx)
{
    return rs_dns_state_get_tx_detect_state(tx);
}

static int RustDNSSetTxDetectState(void *tx,
        DetectEngineState *s)
{
    rs_dns_state_set_tx_detect_state(tx, s);
    return 0;
}

static AppLayerDecoderEvents *RustDNSGetEvents(void *tx)
{
    return rs_dns_state_get_events(tx);
}

void RegisterDNSTCPParsers(void)
{
    const char *proto_name = "dns";

    /** DNS */
    if (AppLayerProtoDetectConfProtoDetectionEnabled("tcp", proto_name)) {
        AppLayerProtoDetectRegisterProtocol(ALPROTO_DNS, proto_name);

        if (RunmodeIsUnittests()) {
            AppLayerProtoDetectPPRegister(IPPROTO_TCP, "53", ALPROTO_DNS, 0,
                    sizeof(DNSHeader) + 2, STREAM_TOSERVER, RustDNSTCPProbe,
                    RustDNSTCPProbe);
        } else {
            int have_cfg = AppLayerProtoDetectPPParseConfPorts("tcp",
                    IPPROTO_TCP, proto_name, ALPROTO_DNS, 0,
                    sizeof(DNSHeader) + 2, RustDNSTCPProbe, RustDNSTCPProbe);
            /* if we have no config, we enable the default port 53 */
            if (!have_cfg) {
                SCLogWarning(SC_ERR_DNS_CONFIG, "no DNS TCP config found, "
                                                "enabling DNS detection on "
                                                "port 53.");
                AppLayerProtoDetectPPRegister(IPPROTO_TCP, "53", ALPROTO_DNS, 0,
                        sizeof(DNSHeader) + 2, STREAM_TOSERVER, RustDNSTCPProbe,
                        RustDNSTCPProbe);
            }
        }
    } else {
        SCLogInfo("Protocol detection and parser disabled for %s protocol.",
                  proto_name);
        return;
    }

    if (AppLayerParserConfParserEnabled("tcp", proto_name)) {
        AppLayerParserRegisterParser(IPPROTO_TCP, ALPROTO_DNS, STREAM_TOSERVER,
                RustDNSTCPParseRequest);
        AppLayerParserRegisterParser(IPPROTO_TCP , ALPROTO_DNS, STREAM_TOCLIENT,
                RustDNSTCPParseResponse);
        AppLayerParserRegisterStateFuncs(IPPROTO_TCP, ALPROTO_DNS,
                rs_dns_state_tcp_new, rs_dns_state_free);
        AppLayerParserRegisterTxFreeFunc(IPPROTO_TCP, ALPROTO_DNS,
                RustDNSStateTransactionFree);
        AppLayerParserRegisterGetEventsFunc(IPPROTO_TCP, ALPROTO_DNS,
                RustDNSGetEvents);
        AppLayerParserRegisterDetectStateFuncs(IPPROTO_TCP, ALPROTO_DNS,
                RustDNSGetTxDetectState, RustDNSSetTxDetectState);
        AppLayerParserRegisterGetTx(IPPROTO_TCP, ALPROTO_DNS, RustDNSGetTx);
        AppLayerParserRegisterGetTxCnt(IPPROTO_TCP, ALPROTO_DNS,
                RustDNSGetTxCnt);
        AppLayerParserRegisterLoggerFuncs(IPPROTO_TCP, ALPROTO_DNS,
                RustDNSGetTxLogged, RustDNSSetTxLogged);
        AppLayerParserRegisterGetStateProgressFunc(IPPROTO_TCP, ALPROTO_DNS,
                RustDNSGetAlstateProgress);
        AppLayerParserRegisterGetStateProgressCompletionStatus(ALPROTO_DNS,
                rs_dns_state_progress_completion_status);
        DNSAppLayerRegisterGetEventInfo(IPPROTO_TCP, ALPROTO_DNS);
        DNSAppLayerRegisterGetEventInfoById(IPPROTO_TCP, ALPROTO_DNS);

        /* This parser accepts gaps. */
        AppLayerParserRegisterOptionFlags(IPPROTO_TCP, ALPROTO_DNS,
                APP_LAYER_PARSER_OPT_ACCEPT_GAPS);

    } else {
        SCLogInfo("Parsed disabled for %s protocol. Protocol detection"
                  "still on.", proto_name);
    }

#ifdef UNITTESTS
    AppLayerParserRegisterProtocolUnittests(IPPROTO_TCP, ALPROTO_DNS,
            RustDNSTCPParserRegisterTests);
#endif

    return;
}

#ifdef UNITTESTS

#include "util-unittest-helper.h"

static int RustDNSTCPParserTestMultiRecord(void)
{
    /* This is a buffer containing 20 DNS requests each prefixed by
     * the request length for transport over TCP.  It was generated with Scapy,
     * where each request is:
     *    DNS(id=i, rd=1, qd=DNSQR(qname="%d.google.com" % i, qtype="A"))
     * where i is 0 to 19.
     */
    uint8_t req[] = {
        0x00, 0x1e, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x30,
        0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x1e, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x31,
        0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x1e, 0x00, 0x02, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x32,
        0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x1e, 0x00, 0x03, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x33,
        0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x1e, 0x00, 0x04, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x34,
        0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x1e, 0x00, 0x05, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x35,
        0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x1e, 0x00, 0x06, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x36,
        0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x1e, 0x00, 0x07, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x37,
        0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x1e, 0x00, 0x08, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x38,
        0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x1e, 0x00, 0x09, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x39,
        0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x1f, 0x00, 0x0a, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x31,
        0x30, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65,
        0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00,
        0x01, 0x00, 0x1f, 0x00, 0x0b, 0x01, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
        0x31, 0x31, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c,
        0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01,
        0x00, 0x01, 0x00, 0x1f, 0x00, 0x0c, 0x01, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x31, 0x32, 0x06, 0x67, 0x6f, 0x6f, 0x67,
        0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00,
        0x01, 0x00, 0x01, 0x00, 0x1f, 0x00, 0x0d, 0x01,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x02, 0x31, 0x33, 0x06, 0x67, 0x6f, 0x6f,
        0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00,
        0x00, 0x01, 0x00, 0x01, 0x00, 0x1f, 0x00, 0x0e,
        0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x02, 0x31, 0x34, 0x06, 0x67, 0x6f,
        0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d,
        0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x1f, 0x00,
        0x0f, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x02, 0x31, 0x35, 0x06, 0x67,
        0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f,
        0x6d, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x1f,
        0x00, 0x10, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x31, 0x36, 0x06,
        0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63,
        0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
        0x1f, 0x00, 0x11, 0x01, 0x00, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x31, 0x37,
        0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x00, 0x1f, 0x00, 0x12, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x31,
        0x38, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65,
        0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00,
        0x01, 0x00, 0x1f, 0x00, 0x13, 0x01, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
        0x31, 0x39, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c,
        0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01,
        0x00, 0x01
    };
    size_t reqlen = sizeof(req);

    RSDNSState *state = rs_dns_state_new();

    Flow *f = UTHBuildFlow(AF_INET, "1.2.3.4", "1.2.3.5", 1024, 53);
    FAIL_IF_NULL(f);
    f->proto = IPPROTO_TCP;
    f->alproto = ALPROTO_DNS;
    f->alstate = state;

    FAIL_IF(RustDNSTCPParseRequest(f, f->alstate, NULL, req, reqlen,
                    NULL, STREAM_START) < 0);
    FAIL_IF(rs_dns_state_get_tx_count(state) != 20);

    UTHFreeFlow(f);
    PASS;
}

static void RustDNSTCPParserRegisterTests(void)
{
    UtRegisterTest("RustDNSTCPParserTestMultiRecord",
            RustDNSTCPParserTestMultiRecord);
}

#endif /* UNITTESTS */
