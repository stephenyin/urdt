/*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "tunnel.h"
#include "codec.h"
#include "operators.h"
#include "transmitter.h"
#include "vassert.h"

extern ecRdtInitializer g_rdtOpendCallback;
extern struct rdt_enc_ops rdt_enc_ops;

int32_t _handshake_request(struct rdt_tunnel* ptunnel)
{
    struct rdt_handshake_req_msg msg;
    char* buf = (char*)alloca(sizeof(msg) + 4);
    int len = 0;

    vassert(ptunnel);
    vlogI("rdt ops: prepare for handshake request (teid:%d)", ptunnel->teid);

    memset(&msg, 0, sizeof(msg));
    msg.type    = CTRL_MSG;
    msg.ctrlId  = 0;
    msg.rteid   = 0;
    msg.version = RDT_VERSION;
    msg.handshake_type = 0;
    msg.lteid   = ptunnel->teid;
    msg.mtu     = RDT_MTU;
    msg.seq     = ptunnel->seq_num;
    msg.windowsz = ptunnel->rxq.max_pkt_num;

    memset(buf, 0, sizeof(msg) + 4);
    len = rdt_enc_ops.handshake_req((struct rdt_common_msg*)&msg, buf, sizeof(msg) + 4);
    session_write(ptunnel->sessionId, ptunnel->channelId, (void*)buf, len);

    ptunnel->state = RDT_STATE_HANDSHAKE_REQ_SENT;
    vtimer_restart(&ptunnel->timer, (ptunnel->timeout_counter + 1) * RDT_HANDSHAKE_TIMEOUT, 0);

    return 0;
}

int32_t _handshake_response(struct rdt_tunnel* ptunnel)
{
    struct rdt_handshake_rsp_msg msg;
    char* buf = (char*)alloca(sizeof(msg));
    int len = 0;

    vassert(ptunnel);
    vlogI("rdt ops: prepare for handshake response (teid:%d)", ptunnel->teid);
    //todo: check version.

    msg.type = CTRL_MSG;
    msg.ctrlId = 0;
    msg.rteid  = ptunnel->peer_teid;
    msg.version = RDT_VERSION;
    msg.handshake_type = 0x01;
    msg.lteid  = ptunnel->teid;
    msg.seq = ptunnel->seq_num;
    msg.seq_ack = ptunnel->ctrl_ack_num;
    msg.mtu = RDT_MTU;
    msg.windowsz = ptunnel->rxq.max_pkt_num;

    memset(buf, 0, sizeof(msg));
    len = rdt_enc_ops.handshake_rsp((struct rdt_common_msg*)&msg, buf, sizeof(msg));
    session_write(ptunnel->sessionId, ptunnel->channelId, (void*)buf, len);

    ptunnel->state = RDT_STATE_HANDSHAKE_RESP_SENT;
    vtimer_restart(&ptunnel->timer, (ptunnel->timeout_counter + 1) * RDT_HANDSHAKE_TIMEOUT, 0);

    return 0;
}

int32_t _handshake_finish(struct rdt_tunnel* ptunnel)
{
    struct rdt_handshake_fin_msg msg;
    char* buf = (char*)alloca(sizeof(msg));
    int len = 0;

    vassert(ptunnel);
    vlogI("rdt ops: ending the handshake negotiation (teid:%d)", ptunnel->teid);

    //TODO check version

    msg.type = CTRL_MSG;
    msg.ctrlId = 0;
    msg.rteid  = ptunnel->peer_teid;
    msg.version = RDT_VERSION;
    msg.handshake_type = 0x02;
    msg.lteid  = ptunnel->teid;
    msg.seq    = ptunnel->seq_num;
    msg.seq_ack = ptunnel->ctrl_ack_num;

    memset(buf, 0, sizeof(msg));
    len = rdt_enc_ops.handshake_fin((struct rdt_common_msg*)&msg, buf, sizeof(msg));
    session_write(ptunnel->sessionId, ptunnel->channelId, (void*)buf, len);

    ptunnel->state = RDT_STATE_READY;
    vcond_signal(&ptunnel->cond);
    vtimer_restart(&ptunnel->timer, RDT_KEEPALIVE_TIMEOUT, 0);

    return 0;
}

int32_t _handshake_delayed_finish(struct rdt_tunnel* ptunnel)
{
    ecRdtHandler* handler = NULL;
    vassert(ptunnel);
    vlogI("rdt ops: handle handshake delayed finish (teid:%d)", ptunnel->teid);

    ptunnel->state = RDT_STATE_READY;

    //Initiating rdt tunnel by peer
    handler = g_rdtOpendCallback.onRdtOpened(ptunnel->sessionId, ptunnel->channelId, ptunnel->teid);
    if ((!handler) ||(!handler->onData) || (!handler->onClosed)) {
        destroy_tunnel(ptunnel, 1);
        return -1;
    }

    ptunnel->handler.onData   = handler->onData;
    ptunnel->handler.onClosed = handler->onClosed;

    vtimer_restart(&ptunnel->timer, RDT_KEEPALIVE_TIMEOUT, 0);
    return 0;
}

int32_t _transfer_send_data(struct rdt_tunnel* ptunnel, const void* data, int length)
{
    data_encoded_pkt_t* encoded_pkt = NULL;
    struct rdt_data_msg msg;
    char* buf = NULL;
    int bufsz = sizeof(msg) + length - 4 - sizeof(void*);
    int len = 0;

    vassert(ptunnel);
    vassert(data);
    vassert(length > 0);

    buf = (char*)malloc(bufsz);
    if (!buf) {
        return ECRDT_E_OOM;
    }

    encoded_pkt = (data_encoded_pkt_t*)malloc(sizeof(*encoded_pkt));
    if (!encoded_pkt) {
        free(buf);
        return ECRDT_E_OOM;
    }

    memset(&msg, 0, sizeof(msg));
    msg.type = DATA_MSG;

    vlock_enter(&ptunnel->lock);
    msg.rteid = ptunnel->peer_teid;
    msg.seq   = ptunnel->seq_num;
    ptunnel->seq_num += length;
    vlock_leave(&ptunnel->lock);
    msg.len   = length;
    msg.data  = (void*)data;

    memset(buf, 0, bufsz);
    len = rdt_enc_ops.data((struct rdt_common_msg*)&msg, buf, bufsz);

    encoded_pkt->data = (void*)buf;
    encoded_pkt->len  = len;
    encoded_pkt->seq  = msg.seq;

    ptunnel->txq.push_pkt((void*)&ptunnel->txq, encoded_pkt);


    return 0;
}

int32_t _transfer_send_data_ack(struct rdt_tunnel* ptunnel, uint32_t ack_num)
{
    struct rdt_data_ack_msg msg;
    char* buf = (char*)alloca(sizeof(msg));
    int len = 0;

    vassert(ptunnel);
    //vlogI("OPERATORS: %s", __FUNCTION__);

    msg.type   = CTRL_MSG;
    msg.ctrlId = 0x02;
    msg.rteid  = ptunnel->peer_teid;
    msg.seq_ack = ack_num;
    msg.windowsz = ptunnel->rxq.max_pkt_num - ptunnel->rxq.cur_pkt_num;

    memset(buf, 0, sizeof(msg));
    len = rdt_enc_ops.data_ack((struct rdt_common_msg*)&msg, buf, sizeof(msg));
    session_write(ptunnel->sessionId, ptunnel->channelId, (void*)buf, len);

    return 0;
}

int32_t _transfer_send_data_fin(struct rdt_tunnel* ptunnel)
{
    vassert(ptunnel != NULL);
    return 0;
}

int32_t _transfer_keepalive(struct rdt_tunnel* ptunnel)
{
    struct rdt_keepalive_msg msg;
    char* buf = (char*)alloca(sizeof(msg));
    int len = 0;

    vassert(ptunnel);
    vlogI("OPERATORS: %s", __FUNCTION__);

    msg.type   = CTRL_MSG;
    msg.ctrlId = 0x01;
    msg.rteid  = ptunnel->peer_teid;

    memset(buf, 0, sizeof(msg));
    len = rdt_enc_ops.keepalive((struct rdt_common_msg*)&msg, buf, sizeof(msg));
    session_write(ptunnel->sessionId, ptunnel->channelId, (void*)buf, len);

    return 0;
}

int32_t _transfer_keepalive_recv(struct rdt_tunnel* ptunnel)
{
    vassert(ptunnel);
    vlogI("OPERATORS: %s", __FUNCTION__);
    return -1;
}

int32_t _shutdown_tunnel(struct rdt_tunnel* ptunnel)
{
    struct rdt_shutdown_msg msg;
    char* buf = (char*)alloca(sizeof(msg));
    int len = 0;

    vassert(ptunnel);
    vlogI("OPERATORS: %s", __FUNCTION__);

    msg.type   = CTRL_MSG;
    msg.ctrlId = 0x03;
    msg.rteid  = ptunnel->peer_teid;

    memset(buf, 0, sizeof(msg));
    len = rdt_enc_ops.shutdown((struct rdt_common_msg*)&msg, buf, sizeof(msg));
    session_write(ptunnel->sessionId, ptunnel->channelId, (void*)buf, len);

    return 0;
}

int32_t _shutdown_tunnel_recv(struct rdt_tunnel* ptunnel)
{
    vassert(ptunnel != NULL);
    vlogI("OPERATORS: %s", __FUNCTION__);
    ptunnel->state = RDT_STATE_CLOSED;
    destroy_tunnel(ptunnel, 0);

    return 0;
}

