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

#include "vlist.h"
#include "headers.h"
#pragma pack(1)

#ifndef __RDT_CODEC_H__
#define __RDT_CODEC_H__

#define DATA_MSG 0x0
#define CTRL_MSG 0x1

enum {
    CTRL_MSG_HANDSHAKE = ((uint8_t)0x0),
    CTRL_MSG_KEEPALIVE = ((uint8_t)0x1),
    CTRL_MSG_ACK       = ((uint8_t)0x2),
    CTRL_MSG_SHUTDOWN  = ((uint8_t)0x3),
    CTRL_MSG_BUTT
};

enum {
    CTRL_MSG_HANDSHAKE_REQ  = ((uint8_t)0x0),
    CTRL_MSG_HANDSHAKE_RESP = ((uint8_t)0x1),
    CTRL_MSG_HANDSHAKE_FIN  = ((uint8_t)0x2),
    CTRL_MSG_HANDSHAKE_BUTT
};

#define HANDSHAKE_REQ_MAGIC ((uint32_t)0xB532A79B)

#define RDT_MSG_HEADER \
    uint8_t type:1; \
    uint8_t ctrlId:7; \
    uint8_t padx; \
    uint16_t rteid

#define RDT_HANDSHAKE_MSG_HEADER \
    RDT_MSG_HEADER; \
    uint16_t version:14; \
    uint16_t handshake_type:2; \
    uint16_t lteid \

struct rdt_common_msg {
    RDT_MSG_HEADER;
};

struct rdt_data_msg {
    RDT_MSG_HEADER;
    uint32_t seq;
    int32_t len;
    void*  data;
//    uint32_t data[1];
};

struct rdt_data_ack_msg {
    RDT_MSG_HEADER;
    uint32_t seq_ack;
    uint32_t windowsz;
};

struct rdt_keepalive_msg {
    RDT_MSG_HEADER;
};

struct rdt_shutdown_msg {
    RDT_MSG_HEADER;
};

struct rdt_handshake_req_msg {
    RDT_HANDSHAKE_MSG_HEADER;
    uint32_t seq;
    uint32_t pad1;
    uint32_t mtu;
    uint32_t windowsz;
};

struct rdt_handshake_rsp_msg {
    RDT_HANDSHAKE_MSG_HEADER;
    uint32_t seq;
    uint32_t seq_ack;
    uint32_t mtu;
    uint32_t windowsz;
};

struct rdt_handshake_fin_msg {
    RDT_HANDSHAKE_MSG_HEADER;
    uint32_t seq;
    uint32_t seq_ack;
};

struct rdt_enc_ops {
    int (*data)         (struct rdt_common_msg*, char*, int);
    int (*data_ack)     (struct rdt_common_msg*, char*, int);
    int (*keepalive)    (struct rdt_common_msg*, char*, int);
    int (*shutdown)     (struct rdt_common_msg*, char*, int);
    int (*handshake_req)(struct rdt_common_msg*, char*, int);
    int (*handshake_rsp)(struct rdt_common_msg*, char*, int);
    int (*handshake_fin)(struct rdt_common_msg*, char*, int);
};

struct rdt_dec_ops {
    int (*data)         (char*, int, struct rdt_common_msg*);
    int (*data_ack)     (char*, int, struct rdt_common_msg*);
    int (*keepalive)    (char*, int, struct rdt_common_msg*);
    int (*shutdown)     (char*, int, struct rdt_common_msg*);
    int (*handshake_req)(char*, int, struct rdt_common_msg*);
    int (*handshake_rsp)(char*, int, struct rdt_common_msg*);
    int (*handshake_fin)(char*, int, struct rdt_common_msg*);
};

typedef struct data_encoded_pkt{
    uint32_t seq;
    uint32_t len;
    uint8_t* data;
} data_encoded_pkt_t;

typedef struct data_pkt{
    struct vlist list;
    uint32_t seq;
    uint16_t teid;
    uint16_t len;
    uint8_t* data;
} data_pkt_t;

#endif
#pragma pack()

