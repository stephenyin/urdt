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

#include <arpa/inet.h>
#include "tunnel.h"
#include "vsys.h"
#include "vassert.h"

typedef void(*HANDLER_PTR)(int, int, char*, int);
extern struct rdt_dec_ops rdt_dec_ops;

static
void handle_handshake_req(int sessionId, int channelId, char* buf, int length)
{
    struct rdt_handshake_req_msg msg;
    rdt_tunnel_t* ptunnel = NULL;
    int ret = 0;

    vassert(sessionId > 0);
    vassert(channelId > 0);
    vassert(length > 0);

    if (length < sizeof(msg)) {
        vlogE("Receiver: invalid handshake_req msg");
        return;
    }

    rdt_dec_ops.handshake_req((char*)buf, length, (struct rdt_common_msg*)&msg);
    if (RDT_VERSION != msg.version) {
        vlogE("Receiver: unsupported version(%d)", msg.version);
        return;
    }

    if(check_peer_teid(sessionId, channelId, msg.lteid) == 1){
        vlogD("sid(%d), cid(%d), peer_teid(%d) handshake req has been received, ignore this.",
              sessionId, channelId, msg.lteid);
        return;
    }

    ret = create_tunnel(sessionId, channelId, NULL, &ptunnel);
    if (ret < 0) {
        vlogE("RECEIVER:Create tunnel failed");
        return;
    }

    vlock_enter(&ptunnel->lock);
    ptunnel->peer_teid = msg.lteid;
    ptunnel->peer_window_sz = msg.windowsz;
    ptunnel->ctrl_ack_num = msg.seq + 1;

    ptunnel->ops[ptunnel->state]->handshake_resp(ptunnel);
    vlock_leave(&ptunnel->lock);
    return;
}

static
void handle_handshake_resp(int sessionId, int channelId, char *buf, int length)
{
    struct rdt_handshake_rsp_msg msg;
    rdt_tunnel_t* ptunnel = NULL;

    vassert(sessionId > 0);
    vassert(channelId > 0);
    vassert(length > 0);

    if (length < sizeof(msg)) {
        vlogE("Receiver: invalid handshake_rsp msg");
        return;
    }

    rdt_dec_ops.handshake_rsp((char*)buf, length, (struct rdt_common_msg*)&msg);
    ptunnel = get_tunnel(msg.rteid);
    if(!ptunnel) {
        vlogE("Receiver: teid (%d) not found", msg.rteid);
        return;
    }

    vlock_enter(&ptunnel->lock);
    if(ptunnel->state != RDT_STATE_HANDSHAKE_REQ_SENT){
        vlock_leave(&ptunnel->lock);
        return;
    }

    ptunnel->timeout_counter = 0;
    ptunnel->peer_teid = msg.lteid;
    ptunnel->peer_window_sz = msg.windowsz;
    ptunnel->ctrl_ack_num = msg.seq + 1;
    ptunnel->seq_num++;

    ptunnel->ops[ptunnel->state]->handshake_fin(ptunnel);
    vlock_leave(&ptunnel->lock);
    return ;
}

static
void handle_handshake_fin(int sessionId, int channelId, char *buf, int length)
{
    struct rdt_handshake_fin_msg msg;
    rdt_tunnel_t* ptunnel = NULL;

    vassert(sessionId > 0);
    vassert(channelId > 0);
    vassert(length > 0);

    if (length < sizeof(msg)) {
        vlogE("Receiver: invalid handshake_fin msg");
        return;
    }

    rdt_dec_ops.handshake_fin((char*)buf, length, (struct rdt_common_msg*)&msg);
    ptunnel = get_tunnel(msg.rteid);
    if (!ptunnel) {
        vlogE("Receiver: teid (%d) not found", msg.rteid);
        return;
    }

    vlock_enter(&ptunnel->lock);
    if(ptunnel->state != RDT_STATE_HANDSHAKE_RESP_SENT){
        vlock_leave(&ptunnel->lock);
        return;
    }

    ptunnel->timeout_counter = 0;
    ptunnel->seq_num++;
    ptunnel->ops[ptunnel->state]->handshake_delayed_fin(ptunnel);
    vlock_leave(&ptunnel->lock);
    return ;
}

static
HANDLER_PTR handshake_msg_handlers[] = {
    handle_handshake_req,  // CTRL_MSG_HANDSHAKE_REQ
    handle_handshake_resp, // CTRL_MSG_HANDSHAKE_RESP
    handle_handshake_fin   // CTRL_MSG_HANDSHAKE_FIN
};

static
void handle_handshake(int sessionId, int channelId, char *buf, int length)
{
    int type = 0;

    vassert(sessionId > 0);
    vassert(channelId > 0);
    vassert(length > 0);

    {
        int off = 0;
        off += sizeof(uint32_t);
        type = ntohs(*(uint16_t*)((char*)buf + off)) >> 14;
    }


    if ((type < 0) ||
        (type > CTRL_MSG_HANDSHAKE_FIN)) {
        vlogE("Receiver: Invalid handshake msg type(%d)", type);
        return;
    }

    handshake_msg_handlers[type](sessionId, channelId, buf, length);
    return ;
}

static
void handle_keepalive(int sessionId, int channelId, char *buf, int length)
{
    struct rdt_keepalive_msg msg;
    rdt_tunnel_t* ptunnel = NULL;

    vassert(sessionId > 0);
    vassert(channelId > 0);
    vassert(length > 0);

    rdt_dec_ops.keepalive((char*)buf, length, (struct rdt_common_msg*)&msg);
    ptunnel = get_tunnel(msg.rteid);
    if(!ptunnel){
        vlogE("Receiver: teid(%d) not found", msg.rteid);
        return;
    }

    vlock_enter(&ptunnel->lock);
    if(ptunnel->state != RDT_STATE_READY){
        vlogE("Receive keepalive on wrong state(%d)", ptunnel->state);
        vlock_leave(&ptunnel->lock);
        return;
    }

    ptunnel->timeout_counter = 0;
    ptunnel->ops[ptunnel->state]->keepalive_recv(ptunnel);

    vlock_leave(&ptunnel->lock);
    return ;
}

static
void handle_shutdown(int sessionId, int channelId, char *buf, int length)
{
    struct rdt_shutdown_msg msg;
    rdt_tunnel_t *ptunnel = NULL;

    vassert(sessionId > 0);
    vassert(channelId > 0);
    vassert(length > 0);

    rdt_dec_ops.shutdown((char*)buf, length, (struct rdt_common_msg*)&msg);
    ptunnel = get_tunnel(msg.rteid);
    if (!ptunnel) {
        vlogE("Reciever: teid(%d) not found", msg.rteid);
        return;
    }

    if(ptunnel->state == RDT_STATE_CLOSED){
        return;
    }
    ptunnel->ops[ptunnel->state]->shutdown_recv(ptunnel);
    return ;
}

static
void handle_data_ack(int sessionId, int channelId, char *buf, int length)
{
    struct rdt_data_ack_msg msg;
    rdt_tunnel_t* ptunnel = NULL;

    vassert(sessionId > 0);
    vassert(channelId > 0);
    vassert(length > 0);

    rdt_dec_ops.data_ack((char*)buf, length, (struct rdt_common_msg*)&msg);
    ptunnel = get_tunnel(msg.rteid);
    if (!ptunnel) {
        vlogE("Receiver: teid(%d) not found", msg.rteid);
        return;
    }

    vlock_enter(&ptunnel->lock);
    if(ptunnel->state != RDT_STATE_READY){
        vlock_leave(&ptunnel->lock);
        vlogE("RECEIVER:: Receive data ack on wrong state(%d)", ptunnel->state);
        return;
    }

    if(ptunnel->data_sending == 1){
        vtimer_restart(&ptunnel->timer, RDT_DATA_ACK_TIMEOUT, 0);
    } else{
        vtimer_restart(&ptunnel->timer, RDT_KEEPALIVE_TIMEOUT, 0);
    }

    ptunnel->timeout_counter = 0;
    ptunnel->peer_window_sz  = msg.windowsz;

    vlock_leave(&ptunnel->lock);
    ptunnel->txq.update_ack(&ptunnel->txq, msg.seq_ack);

    return ;
}

static
HANDLER_PTR ctrl_msg_handlers[] = {
    handle_handshake, // CTRL_MSG_HANDSHAKE
    handle_keepalive, // CTRL_MSG_KEEPALIVE
    handle_data_ack,  // CTRL_MSG_ACK
    handle_shutdown,  // CTRL_MSG_SHUTDOWN
};

static
void handle_data(int sessionId, int channelId, void *buf, int length)
{
    struct rdt_data_msg msg;
    rdt_tunnel_t* ptunnel = NULL;
    data_pkt_t* pkt = NULL;
    int ret = 0;
    uint32_t ack_seq = 0;

    vassert(sessionId > 0);
    vassert(channelId > 0);
    vassert(length > 0);

    pkt = (data_pkt_t*)malloc(sizeof(*pkt) + length -8);
    if (!pkt) {
        vlogE("Failed to malloc data packet\n");
        return ;
    }
    memset(pkt, 0, sizeof(*pkt));
    vlist_init(&pkt->list);
    pkt->data = (uint8_t*)(pkt + 1);

    memset(&msg, 0, sizeof(msg));
    msg.data = pkt->data;

    rdt_dec_ops.data((char*)buf, length, (struct rdt_common_msg*)&msg);
    pkt->seq = msg.seq;
    pkt->len = msg.len;
    pkt->teid = msg.rteid;

    ptunnel = get_tunnel(msg.rteid);
    if (!ptunnel) {
        vlogE("Receiver:Teid(%d) not found", msg.rteid);
        free(pkt);
        return;
    }
    if(ptunnel->state != RDT_STATE_READY){
        vlogE("Receive data on wrong state(%d)", ptunnel->state);
        free(pkt);
        return;
    }
    vtimer_restart(&ptunnel->timer, RDT_KEEPALIVE_TIMEOUT, 0);
    ptunnel->timeout_counter = 0;

    ack_seq = ptunnel->rxq.arrange_pkt(&ptunnel->rxq, pkt);
    ret = ptunnel->ops[ptunnel->state]->send_data_ack(ptunnel, ack_seq);
    if (ret < 0) {
        free(pkt);
    }
    return;
}

void on_session_data(int sessionId, int channelId, char* buf, int length)
{
    uint8_t  msgtype  = 0;
    uint8_t  ctrltype = 0;
    int off = 0;

    vassert(buf != NULL);
    vassert(length >= 4);

    if (HANDSHAKE_REQ_MAGIC == ntohl(*(uint32_t*)buf)) {
        off += sizeof(uint32_t);
    }

    msgtype  = (uint8_t)(*(uint8_t*)(buf + off) & 0x01);
    ctrltype = (uint8_t)(*(uint8_t*)(buf + off) >> 1);

    if (msgtype == DATA_MSG) {
        handle_data(sessionId, channelId, buf + off, length);
    } else if ((ctrltype >= 0) && (ctrltype <= CTRL_MSG_SHUTDOWN)) {
        ctrl_msg_handlers[ctrltype](sessionId, channelId, buf + off, length);
    } else {
        vlogE("Receiver: Unrecognized msg.");
    }
    return ;
}

