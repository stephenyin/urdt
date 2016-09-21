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

#ifndef __ECRDT_TUNNEL_H__
#define __ECRDT_TUNNEL_H__

#include "codec.h"
#include "vlist.h"
#include "rxq.h"
#include "txq.h"
#include "ecRdt.h"

#define RDT_VERSION 0x01
#define RDT_MTU 1500

#define RDT_HANDSHAKE_TIMEOUT 2
#define RDT_HANDSHAKE_TIMEOUT_LIMITATION 3
#define TUNNEL_OPEN_TIMEOUT (RDT_HANDSHAKE_TIMEOUT * RDT_HANDSHAKE_TIMEOUT_LIMITATION * 2)

#define RDT_KEEPALIVE_TIMEOUT 45
#define RDT_KEEPALIVE_TIMEOUT_LIMITATION 9

#define RDT_DATA_ACK_TIMEOUT 1
#define RDT_DATA_ACK_TIMEOUT_LIMITATION 90

#define MAX_TUNNEL_NUM_PER_CHANNEL 5

enum {
    RDT_STATE_HANDSHAKE_REQ_SENT = 0,
    RDT_STATE_HANDSHAKE_RESP_SENT,
    RDT_STATE_READY,
    RDT_STATE_CLOSED,
    RDT_STATE_BUTT
};

struct rdt_tunnel;

typedef void (*upper_data_cb)(int, void*, int);

struct rdt_proto_ops {
    int32_t (*handshake_req)(struct rdt_tunnel*);
    int32_t (*handshake_resp)(struct rdt_tunnel*);
    int32_t (*handshake_fin)(struct rdt_tunnel*);
    int32_t (*handshake_delayed_fin)(struct rdt_tunnel*);

    int32_t (*send_data)(struct rdt_tunnel*, const void* data, int32_t length);
    int32_t (*send_data_ack)(struct rdt_tunnel*, uint32_t ack_num);
    int32_t (*send_data_fin)(struct rdt_tunnel*);

    int32_t (*shutdown)(struct rdt_tunnel*);
    int32_t (*shutdown_recv)(struct rdt_tunnel*);

    int32_t (*keepalive)(struct rdt_tunnel*);
    int32_t (*keepalive_recv)(struct rdt_tunnel*);
};

typedef struct rdt_tunnel {
    struct vlist list;
    struct vlock lock;
    struct vcond cond;
    struct vtimer timer;
    struct vthread rx_data_dispatcher;
    struct vthread tx_data_dispatcher;

    int32_t state;
    int32_t teid;                        //Tunnel Endpoint Identifier
    int32_t peer_teid;              //Peer Tunnel Endpoint Identifier
    uint64_t tx_bytes;              //Statistics transmit bytes
    uint64_t rx_bytes;              //Statistics receive bytes
    int32_t sessionId;
    int32_t channelId;
    uint32_t seq_num;                   //The seq num which should be present in next pkt
    uint32_t ctrl_ack_num;          //The ack num in handshake period
    uint32_t peer_window_sz;    //Peer available buffer size(pkt num)
    int32_t timeout_counter;
    int8_t data_sending;           //Indicate tunnel is in data sending state or not
    int8_t rx_dispatcher_run;  //Thread running flag
    int8_t tx_dispatcher_run;  //Thread running flag
    int8_t fwd_data2upper;      //The flag which indicates if forward data to upper protocol stack (port-forwarding etc.)
    upper_data_cb on_upper_data;   //The on data callback function upper protocol set to rdt
    ecRdtHandler handler;

    tx_pkt_mngr_t txq;
    rx_pkt_mngr_t rxq;
    struct rdt_proto_ops* ops[RDT_STATE_BUTT];
    struct rdt_proto_enc_ops* enc_ops;
    struct rdt_proto_dec_ops* dec_ops;
} rdt_tunnel_t;

int create_tunnel(int32_t sessionId, int32_t channelId, ecRdtHandler* handler, rdt_tunnel_t**);
void destroy_tunnel(struct rdt_tunnel* prt, int send_shutdown);
struct rdt_tunnel* get_tunnel(int32_t teid);
void destroy_all_tunnel();
int32_t tunnel_send_data(struct rdt_tunnel* ptunnel, const void* data, int32_t len);
int32_t check_peer_teid(int32_t sid, int32_t cid, int32_t teid);

#endif

