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
#include "headers.h"
#include "vassert.h"
#include "vsys.h"
#include "vlist.h"
#include "transmitter.h"
#include "receiver.h"

#define PORT_FORWARDING_MAGIC  ((uint32_t)0xA29BF88E)
#define PORT_FORWARDING_MSG_LENGTH 12

extern struct rdt_proto_ops state_ready_ops;
extern struct rdt_proto_ops state_closed_ops;
extern struct rdt_proto_ops state_handshake_req_sent_ops;
extern struct rdt_proto_ops state_handshake_resp_sent_ops;
extern struct rdt_proto_enc_ops enc_ops;
extern struct rdt_proto_dec_ops dec_ops;
extern uint8_t g_rdtInitialized;

extern int session_set_hook(int session_id, int channel_id, int enable, int type);     //rdt: type=0

int rdt_set_cb(void (*cb)(int, void*, int));
int set_rdt_callback_enable(int teid, int8_t enable);
static upper_data_cb s_port_forwarding_cb = NULL;

typedef struct tunnel_mngr{
    struct vlock lock;
    struct vlist tunnel_list;
    int max_tunnel_num;
    int cur_tunnel_num;
} tunnel_mngr_t;

static struct vlock teid_lock = VLOCK_INITIALIZER;
static uint16_t last_teid = 1;
static tunnel_mngr_t tunnel_manager = {
    .lock = VLOCK_INITIALIZER,
    .tunnel_list = {&tunnel_manager.tunnel_list, &tunnel_manager.tunnel_list},
    .max_tunnel_num = 0,
    .cur_tunnel_num = 0
};

static int add_tunnel(struct rdt_tunnel* ptunnel, int*);
static int del_tunnel(struct rdt_tunnel* ptunnel);
static uint16_t generate_local_teid();
static int timeout_handler(void*);
static int rx_data_dispatcher(void* argv);
static int tx_data_dispatcher(void* argv);

int create_tunnel(int sessionId, int channelId, ecRdtHandler* handler, struct rdt_tunnel** tunnel)
{
    struct rdt_tunnel* ptunnel = NULL;
    int first_tunnel = 0;
    int ret = 0;

    vassert(sessionId > 0);
    vassert(channelId > 0);
    vassert(handler || !handler);
    vassert(tunnel);

    vlogD("TUNNEL:create_tunnel session(%d) channel(%d)", sessionId, channelId);

    ptunnel = (rdt_tunnel_t*)malloc(sizeof(rdt_tunnel_t));
    retE((!ptunnel), ECRDT_E_OOM);
    memset(ptunnel, 0, sizeof(rdt_tunnel_t));

    vlist_init(&ptunnel->list);
    ptunnel->state = RDT_STATE_CLOSED;
    ptunnel->sessionId = sessionId;
    ptunnel->channelId = channelId;
    ptunnel->teid = generate_local_teid();
    vlogD("TUNNEL:teid(%d)", ptunnel->teid);

    ret = add_tunnel(ptunnel, &first_tunnel);
    if (ret < 0) {
        vlogE("TUNNEL:There are %d tunnels have been created on session(%d) channel(%d). Reach the limitation!!",
            MAX_TUNNEL_NUM_PER_CHANNEL,
            ptunnel->sessionId, ptunnel->channelId);
        free(ptunnel);
        return ECRDT_E_EXCEED_LIMIT;
    }
    if (first_tunnel) {
        vlogI("TUNNEL:Notify ecSession to add the data callback for session(%d) channel(%d)",
            sessionId, channelId);
        session_set_hook(sessionId, channelId, 1, 0);
    }

    vtimer_init(&ptunnel->timer, &timeout_handler,(void*)ptunnel, 1);
    vlock_init(&ptunnel->lock);
    vcond_init(&ptunnel->cond);
    vthread_init(&ptunnel->tx_data_dispatcher, tx_data_dispatcher, ptunnel);
    vthread_init(&ptunnel->rx_data_dispatcher, rx_data_dispatcher, ptunnel);

    ptunnel->seq_num = 0;
    ptunnel->ctrl_ack_num = -1;
    ptunnel->timeout_counter = 0;
    ptunnel->data_sending = 0;
    ptunnel->tx_bytes = 0;
    ptunnel->rx_bytes = 0;
    ptunnel->fwd_data2upper = 0;
    ptunnel->on_upper_data = NULL;

    ptunnel->ops[RDT_STATE_CLOSED] = &state_closed_ops;
    ptunnel->ops[RDT_STATE_HANDSHAKE_REQ_SENT] = &state_handshake_req_sent_ops;
    ptunnel->ops[RDT_STATE_HANDSHAKE_RESP_SENT] = &state_handshake_resp_sent_ops;
    ptunnel->ops[RDT_STATE_READY] = &state_ready_ops;

    //Init txq
    {
        ptunnel->txq.init = &init_txq;
        ptunnel->txq.deinit = &deinit_txq;
        ptunnel->txq.push_pkt = &push_pkt;
        ptunnel->txq.fetch_pkt = &fetch_txq_pkt;
        ptunnel->txq.update_ack = &update_ack;
        ptunnel->txq.trigger_resend = &trigger_resend;

        ptunnel->txq.init(&ptunnel->txq);
    }

    //Init rxq
    {
        ptunnel->rxq.init = &init_rxq;
        ptunnel->rxq.deinit = &deinit_rxq;
        ptunnel->rxq.arrange_pkt = &arrange_pkt;
        ptunnel->rxq.fetch_pkt = &fetch_rxq_pkt;

        ptunnel->rxq.init(&ptunnel->rxq);
    }

    if(handler != NULL){
        //Initiating rdt tunnel
        ptunnel->handler.onData   = handler->onData;
        ptunnel->handler.onClosed = handler->onClosed;
        ptunnel->ops[ptunnel->state]->handshake_req(ptunnel);

        vlock_enter(&ptunnel->lock);
        vcond_timedwait(&ptunnel->cond, &ptunnel->lock, TUNNEL_OPEN_TIMEOUT);
        vlock_leave(&ptunnel->lock);

        if(ptunnel->state != RDT_STATE_READY) {
            vlogE("Handshake (%d) timeout!!", ptunnel->teid);

            if(del_tunnel(ptunnel) == 0){
                vlogI("TUNNEL:Notify ecSession to remove the data callback for session(%d) channel(%d)",
                    ptunnel->sessionId, ptunnel->channelId);
                session_set_hook(ptunnel->sessionId, ptunnel->channelId, 0, 0);
            }

            vtimer_deinit(&ptunnel->timer);
            vlock_deinit(&ptunnel->lock);
            vcond_deinit(&ptunnel->cond);

            ptunnel->rxq.deinit(&ptunnel->rxq);
            ptunnel->txq.deinit(&ptunnel->txq);

            free(ptunnel);
            return -1;
        }
    }

    ptunnel->tx_dispatcher_run = 1;
    vthread_start(&ptunnel->tx_data_dispatcher);

    ptunnel->rx_dispatcher_run = 1;
    vthread_start(&ptunnel->rx_data_dispatcher);

    *tunnel = ptunnel;
    return 0;
}

void destroy_tunnel(struct rdt_tunnel* ptunnel, int send_shutdown)
{
    int ret_code = 0;
    vassert(ptunnel);

    vlogD("TUNNEL:destroy_tunnel (teid:%d))", ptunnel->teid);

    if(del_tunnel(ptunnel) == 0){
        vlogI("TUNNEL:Notify ecSession to remove the data callback for session(%d) channel(%d)",
            ptunnel->sessionId, ptunnel->channelId);
        session_set_hook(ptunnel->sessionId, ptunnel->channelId, 0, 0);
    }

    if(send_shutdown == 1){
        ptunnel->ops[ptunnel->state]->shutdown(ptunnel);
    }

    if (ptunnel->handler.onClosed) {
        ptunnel->handler.onClosed(ptunnel->teid, 0);
    }

    vlock_enter(&ptunnel->rxq.rx_lock);
    ptunnel->rx_dispatcher_run = 0;
    vcond_signal(&ptunnel->rxq.rx_cond);
    vlock_leave(&ptunnel->rxq.rx_lock);
    vthread_join(&ptunnel->rx_data_dispatcher, &ret_code);

    vlock_enter(&ptunnel->txq.tx_lock);
    ptunnel->tx_dispatcher_run = 0;
    vcond_signal(&ptunnel->txq.tx_cond);
    vlock_leave(&ptunnel->txq.tx_lock);
    vthread_join(&ptunnel->tx_data_dispatcher, &ret_code);

    vtimer_deinit(&ptunnel->timer);
    vlock_deinit(&ptunnel->lock);
    vcond_deinit(&ptunnel->cond);

    ptunnel->rxq.deinit(&ptunnel->rxq);
    ptunnel->txq.deinit(&ptunnel->txq);

    free(ptunnel);
    return;
}

struct rdt_tunnel* get_tunnel(int teid)
{
    struct rdt_tunnel* ptunnel = NULL;
    struct vlist* node = NULL;
    int found = 0;

    if(teid <= 0) {
        vlogE("Wrong rdt teid(%d)", teid);
        return NULL;
    }

    vlock_enter(&tunnel_manager.lock);
    __vlist_for_each(node, &tunnel_manager.tunnel_list) {
        ptunnel = vlist_entry(node, struct rdt_tunnel, list);
        if (ptunnel->teid == teid) {
            found = 1;
            break;
        }
    }
    vlock_leave(&tunnel_manager.lock);
    return (found ? ptunnel : NULL);
}

int check_peer_teid(int sid, int cid, int teid)
{
    struct rdt_tunnel* ptunnel = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(sid > 0);
    vassert(cid > 0);
    vassert(teid > 0);

    vlogD("TUNNEL:check_peer_teid (teid:%d))", teid);

    vlock_enter(&tunnel_manager.lock);
    __vlist_for_each(node, &tunnel_manager.tunnel_list) {
        ptunnel = vlist_entry(node, struct rdt_tunnel, list);
        if (ptunnel->sessionId == sid &&
            ptunnel->channelId == cid &&
            ptunnel->peer_teid == teid) {
            found = 1;
            break;
        }
    }
    vlock_leave(&tunnel_manager.lock);
    return found;
}

int add_tunnel(struct rdt_tunnel* ptunnel, int* first_tunnel)
{
    struct rdt_tunnel* pt = NULL;
    struct vlist* node = NULL;
    int ntunnels = 0;

    vassert(ptunnel);
    vassert(first_tunnel);

    vlogD("TUNNEL:add_tunnel teid(%d)", ptunnel->teid);

    vlock_enter(&tunnel_manager.lock);
    //check if this is the first rdt tunnel on the channel
    __vlist_for_each(node, &tunnel_manager.tunnel_list) {
        pt = vlist_entry(node, struct rdt_tunnel, list);
        if (pt->sessionId == ptunnel->sessionId && pt->channelId == ptunnel->channelId) {
            if(++ntunnels >= MAX_TUNNEL_NUM_PER_CHANNEL) {
                vlock_leave(&tunnel_manager.lock);
                return -1;
            }
        }
    }

    vlist_add_tail(&tunnel_manager.tunnel_list, &ptunnel->list);
    vlock_leave(&tunnel_manager.lock);

    *first_tunnel = !ntunnels;
    return 0;
}

int del_tunnel(struct rdt_tunnel* ptunnel)
{
    struct rdt_tunnel* pt = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(ptunnel);
    vlogD("TUNNEL:del_tunnel teid(%d)", ptunnel->teid);

    vlock_enter(&tunnel_manager.lock);

    vlist_del(&ptunnel->list);
    vlist_init(&ptunnel->list);

    //check if this is the last rdt tunnel on the same channel
    __vlist_for_each(node, &tunnel_manager.tunnel_list) {
        pt = vlist_entry(node, struct rdt_tunnel, list);
        if (pt->sessionId == ptunnel->sessionId && pt->channelId == ptunnel->channelId) {
            found = 1;
            break;
        }
    }
    vlock_leave(&tunnel_manager.lock);

    return found ;
}

uint16_t generate_local_teid()
{
    vlock_enter(&teid_lock);
    uint16_t teid = last_teid;
    last_teid++;
    vlock_leave(&teid_lock);

    return teid;
}

int timeout_handler(void* argv)
{
    rdt_tunnel_t* ptunnel = (rdt_tunnel_t*)argv;
    vassert(ptunnel);
    vlogD("TUNNEL:timeout_handler");

    switch(ptunnel->state){
    case RDT_STATE_HANDSHAKE_REQ_SENT:
        ptunnel->timeout_counter++;
        if(ptunnel->timeout_counter >= RDT_HANDSHAKE_TIMEOUT_LIMITATION){
            destroy_tunnel(ptunnel, 1);
            break;
        }
        ptunnel->ops[ptunnel->state]->handshake_req(ptunnel);
        break;
    case RDT_STATE_HANDSHAKE_RESP_SENT:
        ptunnel->timeout_counter++;
        if(ptunnel->timeout_counter >= RDT_HANDSHAKE_TIMEOUT_LIMITATION){
            destroy_tunnel(ptunnel, 1);
            break;
        }
        ptunnel->ops[ptunnel->state]->handshake_resp(ptunnel);
        break;
    case RDT_STATE_READY:
        ptunnel->timeout_counter++;
        if(ptunnel->data_sending == 1){
            if(ptunnel->timeout_counter >= RDT_DATA_ACK_TIMEOUT_LIMITATION){
                destroy_tunnel(ptunnel, 1);
                return 0;
            }

            if(ptunnel->txq.trigger_resend(&ptunnel->txq) != 0){
                //No data need to be resend. Start keepalive.
                ptunnel->data_sending = 0;
                ptunnel->timeout_counter = 0;
                vtimer_restart(&ptunnel->timer, RDT_KEEPALIVE_TIMEOUT, 0);
            } else {
                // Resending reset Data-ack timeout
                vtimer_restart(&ptunnel->timer, RDT_DATA_ACK_TIMEOUT, 0);
            }
        } else {
            if(ptunnel->timeout_counter >= RDT_KEEPALIVE_TIMEOUT_LIMITATION){
                destroy_tunnel(ptunnel, 1);
                return 0;
            }
            ptunnel->ops[ptunnel->state]->keepalive(ptunnel);
            vtimer_restart(&ptunnel->timer, RDT_KEEPALIVE_TIMEOUT, 0);
        }
        break;
    default:
        vlogE("TUNNEL:Error timeout state(%d)", ptunnel->state);
    }

    return 0;
}

void destroy_all_tunnel()
{
    struct rdt_tunnel* tunnel = NULL;
    struct vlist* node = NULL;

    vlogD("TUNNEL:destroy_all_tunnel");


    vlock_enter(&tunnel_manager.lock);
    while(!vlist_is_empty(&tunnel_manager.tunnel_list)) {
        node = vlist_pop_head(&tunnel_manager.tunnel_list);
        vlock_leave(&tunnel_manager.lock);
        tunnel = vlist_entry(node, struct rdt_tunnel, list);
        vassert(tunnel);
        destroy_tunnel(tunnel, 1);

        vlock_enter(&tunnel_manager.lock);
    }
    vlock_leave(&tunnel_manager.lock);
    return;
}

int tunnel_send_data(struct rdt_tunnel* ptunnel, const void* data, int len)
{
    vassert(ptunnel != NULL);
    vassert(data != NULL);
    vassert(len > 0);

    if(ptunnel->state != RDT_STATE_READY) {
        vlogE("Send data on error state(%d)", ptunnel->state);
        return -1;
    }

    return ptunnel->ops[ptunnel->state]->send_data(ptunnel, data, len);
}

int rx_data_dispatcher(void* argv)
{
    rdt_tunnel_t* ptunnel = (rdt_tunnel_t*)argv;
    data_pkt_t* pkt = NULL;
    int do_fetch = 0;

    vassert(ptunnel);

    while(ptunnel->rx_dispatcher_run){
        while(do_fetch){
            do_fetch = ptunnel->rxq.fetch_pkt(&ptunnel->rxq, &pkt);
            if (do_fetch < 0) { // no packets to fetch.
                break;
            }
            vassert(pkt);
            vassert(pkt->data);

            if ((s_port_forwarding_cb != NULL) &&
                (ptunnel->fwd_data2upper == 0) &&
                (*(uint32_t*)pkt->data == PORT_FORWARDING_MAGIC) &&
                (pkt->len == PORT_FORWARDING_MSG_LENGTH)) { // for upper layer.
                ptunnel->fwd_data2upper = 1;
                ptunnel->on_upper_data  = s_port_forwarding_cb;
            }

            if(ptunnel->fwd_data2upper){
                ptunnel->on_upper_data(ptunnel->teid, (void*)pkt->data, pkt->len);
            } else {
                ptunnel->handler.onData(ptunnel->teid, (void*)pkt->data, pkt->len);
            }

            ptunnel->rx_bytes += pkt->len;
           // free(pkt->data);
            free(pkt);
        }

        if(ptunnel->rx_dispatcher_run) {
            vlock_enter(&ptunnel->rxq.rx_lock);
            vcond_wait(&ptunnel->rxq.rx_cond, &ptunnel->rxq.rx_lock);
            vlock_leave(&ptunnel->rxq.rx_lock);
            do_fetch = 1;
        }
    }
    return 0;
}

int tx_data_dispatcher(void* argv)
{
    rdt_tunnel_t* ptunnel = (rdt_tunnel_t*)argv;
    data_encoded_pkt_t* pkt = NULL;
    vassert(ptunnel);

    while(ptunnel->tx_dispatcher_run){
        while(ptunnel->txq.fetch_pkt(&ptunnel->txq, &pkt)){
            session_write(ptunnel->sessionId, ptunnel->channelId, (void*)pkt->data, pkt->len);
            ptunnel->tx_bytes += pkt->len;
            if(ptunnel->data_sending == 0){
                ptunnel->data_sending = 1;
                vtimer_restart(&ptunnel->timer, RDT_DATA_ACK_TIMEOUT, 0);
            }
        }

        if(ptunnel->tx_dispatcher_run){
            vlock_enter(&ptunnel->txq.tx_lock);
            vcond_wait(&ptunnel->txq.tx_cond, &ptunnel->txq.tx_lock);
            vlock_leave(&ptunnel->txq.tx_lock);
        }
    }

    return 0;
}

int rdt_set_cb(void (*cb)(int, void*, int))
{
    if(g_rdtInitialized == 0){
        return -1;
    }

    s_port_forwarding_cb = cb;
    return 0;
}

int set_rdt_callback_enable(int teid, int8_t enable)
{
    if(g_rdtInitialized == 0){
        return -1;
    }
    if (!s_port_forwarding_cb) {
        return -1;
    }

    rdt_tunnel_t *ptunnel = get_tunnel(teid);
    if(ptunnel == NULL){
        vlogE("RECEIVER:set_rdt_callback_enable Teid(%d) not found", teid);
        return -1;
    }

    ptunnel->fwd_data2upper = !!enable;
    if(ptunnel->fwd_data2upper){
        ptunnel->on_upper_data = s_port_forwarding_cb;
    } else {
        ptunnel->on_upper_data = NULL;
    }
    return 0;
}

