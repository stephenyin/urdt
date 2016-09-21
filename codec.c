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
#include "codec.h"
#include "vassert.h"

static
int _rdt_encode_data_msg(struct rdt_common_msg* cmsg, char* buf, int length)
{
    struct rdt_data_msg* msg = (struct rdt_data_msg*)cmsg;
    int off = 0;

    vassert(msg);
    vassert(buf);
    vassert(length >= (sizeof(*msg) - 4 - sizeof(void*)));

    *(uint8_t*) (buf + off) = 0;
    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    *(uint16_t*)(buf + off) = htons(msg->rteid);
    off += sizeof(uint16_t);

    *(uint32_t*)(buf + off) = htonl(msg->seq);
    off += sizeof(uint32_t);
    memcpy(buf + off, msg->data, length - off);
    off += length - off;

    return off;
}

static
int _rdt_encode_data_ack_msg(struct rdt_common_msg* cmsg, char* buf, int length)
{
    struct rdt_data_ack_msg* msg = (struct rdt_data_ack_msg*)cmsg;
    int off = 0;

    vassert(cmsg);
    vassert(buf);
    vassert(length >= sizeof(*msg));

    *(uint8_t*)(buf + off)  = (uint8_t)(0x01) | (uint8_t)(0x02 << 1);
    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    *(uint16_t*)(buf + off) = htons(msg->rteid);
    off += sizeof(uint16_t);

    *(uint32_t*)(buf + off) = htonl(msg->seq_ack);
    off += sizeof(uint32_t);
    *(uint32_t*)(buf + off) = htonl(msg->windowsz);
    off += sizeof(uint32_t);

    return off;
}

static
int _rdt_encode_keepalive_msg(struct rdt_common_msg* cmsg, char* buf, int length)
{
    struct rdt_keepalive_msg* msg = (struct rdt_keepalive_msg*)cmsg;
    int off = 0;

    vassert(msg);
    vassert(buf);
    vassert(length >= sizeof(*msg));

    *(uint8_t*) (buf + off) = (uint8_t)0x01 | (uint8_t)(0x01 << 1);
    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    *(uint16_t*)(buf + off) = htons(msg->rteid);
    off += sizeof(uint16_t);

    return off;
}

static
int _rdt_encode_shutdown_msg(struct rdt_common_msg* cmsg, char* buf, int length)
{
    struct rdt_shutdown_msg* msg = (struct rdt_shutdown_msg*)cmsg;
    int off = 0;

    vassert(msg);
    vassert(buf);
    vassert(length >= sizeof(*msg));

    *(uint8_t*) (buf + off) = (uint8_t)0x01 | (uint8_t)(0x03 << 1);
    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    *(uint16_t*)(buf + off) = htons(msg->rteid);
    off += sizeof(uint16_t);

    return off;
}

static
int _rdt_encode_handshake_req_msg(struct rdt_common_msg* cmsg, char* buf, int length)
{
    struct rdt_handshake_req_msg* msg = (struct rdt_handshake_req_msg*)cmsg;
    int off = 0;

    vassert(msg);
    vassert(buf);
    vassert(length >= sizeof(*msg) + 4);

    *(uint32_t*)(buf + off) = htonl(HANDSHAKE_REQ_MAGIC);
    off += sizeof(uint32_t);
    *(uint8_t*) (buf + off) = (uint8_t)0x01;
    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    *(uint16_t*)(buf + off) = 0;
    off += sizeof(uint16_t);
    *(uint16_t*)(buf + off) = htons((uint16_t)msg->version | 0x0 << 14);
    off += sizeof(uint16_t);
    *(uint16_t*)(buf + off) = htons(msg->lteid);
    off += sizeof(uint16_t);

    *(uint32_t*)(buf + off) = htonl(msg->seq);
    off += sizeof(uint32_t);
    *(uint32_t*)(buf + off) = 0;
    off += sizeof(uint32_t);
    *(uint32_t*)(buf + off) = htonl(msg->mtu);
    off += sizeof(uint32_t);
    *(uint32_t*)(buf + off) = htonl(msg->windowsz);
    off += sizeof(uint32_t);

    return off;
}

static
int _rdt_encode_handshake_rsp_msg(struct rdt_common_msg* cmsg, char* buf, int length)
{
    struct rdt_handshake_rsp_msg* msg = (struct rdt_handshake_rsp_msg*)cmsg;
    int off = 0;

    vassert(buf);
    vassert(msg);
    vassert(length >= sizeof(*msg));

    *(uint8_t*)(buf + off)  = (uint8_t)0x01;
    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    *(uint16_t*)(buf + off) = htons(msg->rteid);
    off += sizeof(uint16_t);
    *(uint16_t*)(buf + off) = htons((uint16_t)msg->version | (uint16_t)(0x01 << 14));
    off += sizeof(uint16_t);
    *(uint16_t*)(buf + off) = htons(msg->lteid);
    off += sizeof(uint16_t);

    *(uint32_t*)(buf + off) = htonl(msg->seq);
    off += sizeof(uint32_t);
    *(uint32_t*)(buf + off) = htonl(msg->seq_ack);
    off += sizeof(uint32_t);
    *(uint32_t*)(buf + off) = htonl(msg->mtu);
    off += sizeof(uint32_t);
    *(uint32_t*)(buf + off) = htonl(msg->windowsz);
    off += sizeof(uint32_t);

    return off;
}

static
int _rdt_encode_handshake_fin_msg(struct rdt_common_msg* cmsg, char* buf, int length)
{
    struct rdt_handshake_fin_msg* msg = (struct rdt_handshake_fin_msg*)cmsg;
    int off = 0;

    vassert(msg);
    vassert(buf);
    vassert(length >= sizeof(*msg));

    *(uint8_t*) (buf + off) = (uint8_t)0x01;
    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    *(uint16_t*)(buf + off) = htons(msg->rteid);
    off += sizeof(uint16_t);
    *(uint16_t*)(buf + off) = htons((uint16_t)(0x02 << 14));
    off += sizeof(uint16_t);
    off += sizeof(uint16_t);

    *(uint32_t*)(buf + off) = htonl(msg->seq);
    off += sizeof(uint32_t);
    *(uint32_t*)(buf + off) = htonl(msg->seq_ack);
    off += sizeof(uint32_t);

    return off;
}

struct rdt_enc_ops rdt_enc_ops = {
    .data           = _rdt_encode_data_msg,
    .data_ack       = _rdt_encode_data_ack_msg,
    .keepalive      = _rdt_encode_keepalive_msg,
    .shutdown       = _rdt_encode_shutdown_msg,
    .handshake_req  = _rdt_encode_handshake_req_msg,
    .handshake_rsp  = _rdt_encode_handshake_rsp_msg,
    .handshake_fin  = _rdt_encode_handshake_fin_msg
};


static
int _rdt_decode_data_msg(char* buf, int length, struct rdt_common_msg* cmsg)
{
    struct rdt_data_msg* msg = (struct rdt_data_msg*)cmsg;
    int off = 0;

    vassert(buf);
    vassert(length >= (sizeof(*msg) - 4 - sizeof(void*)));
    vassert(msg);

    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    msg->rteid = ntohs(*(uint16_t*)(buf + off));
    off += sizeof(uint16_t);

    msg->seq = ntohl(*(uint32_t*)(buf + off));
    off += sizeof(uint32_t);
    msg->len = length - off;
    memcpy(msg->data, buf + off, msg->len);
    off += length - off;
    return off;
}

static
int _rdt_decode_data_ack_msg(char* buf, int length, struct rdt_common_msg* cmsg)
{
    struct rdt_data_ack_msg* msg = (struct rdt_data_ack_msg*)cmsg;
    int off = 0;

    vassert(buf);
    vassert(length >= sizeof(*msg));
    vassert(msg);

    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    msg->rteid = ntohs(*(uint16_t*)(buf + off));
    off += sizeof(uint16_t);

    msg->seq_ack  = ntohl(*(uint32_t*)(buf + off));
    off += sizeof(uint32_t);
    msg->windowsz = ntohl(*(uint32_t*)(buf + off));
    off += sizeof(uint32_t);

    return off;
}

static
int _rdt_decode_keepalive_msg(char* buf, int length, struct rdt_common_msg* cmsg)
{
    struct rdt_keepalive_msg* msg = (struct rdt_keepalive_msg*)cmsg;
    int off = 0;

    vassert(buf);
    vassert(length >= sizeof(*msg));
    vassert(msg);

    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    msg->rteid = ntohs(*(uint16_t*)(buf + off));
    off += sizeof(uint16_t);

    return off;
}

static
int _rdt_decode_shutdown_msg(char* buf, int length, struct rdt_common_msg* cmsg)
{
    struct rdt_shutdown_msg* msg = (struct rdt_shutdown_msg*)cmsg;
    int off = 0;

    vassert(buf);
    vassert(length >= sizeof(*msg));
    vassert(msg);

    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    msg->rteid = ntohs(*(uint16_t*)(buf + off));
    off += sizeof(uint16_t);

    return off;
}

static
int _rdt_decode_handshake_req_msg(char* buf, int length, struct rdt_common_msg* cmsg)
{
    struct rdt_handshake_req_msg* msg = (struct rdt_handshake_req_msg*)cmsg;
    int off = 0;

    vassert(buf);
    vassert(length >= sizeof(*msg));
    vassert(msg);

    //off += sizeof(uint32_t); // for magic
    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    off += sizeof(uint16_t);

    msg->version = ntohs(*(uint16_t*)(buf + off)) & 0x3f;
    off += sizeof(uint16_t);
    msg->lteid   = ntohs(*(uint16_t*)(buf + off));
    off += sizeof(uint16_t);

    msg->seq = ntohl(*(uint32_t*)(buf + off));
    off += sizeof(uint32_t);
    off += sizeof(uint32_t);
    msg->mtu = ntohl(*(uint32_t*)(buf + off));
    off += sizeof(uint32_t);
    msg->windowsz = ntohl(*(uint32_t*)(buf + off));
    off += sizeof(uint32_t);

    return off;
}

static
int _rdt_decode_handshake_rsp_msg(char* buf, int length, struct rdt_common_msg* cmsg)
{
    struct rdt_handshake_rsp_msg* msg = (struct rdt_handshake_rsp_msg*)cmsg;
    int off = 0;

    vassert(buf);
    vassert(length >= sizeof(*msg));
    vassert(msg);

    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    msg->rteid = ntohs(*(uint16_t*)(buf + off));
    off += sizeof(uint16_t);

    msg->version = ntohs(*(uint16_t*)(buf + off)) & 0x3f;
    off += sizeof(uint16_t);
    msg->lteid  = ntohs(*(uint16_t*)(buf + off));
    off += sizeof(uint16_t);

    msg->seq = ntohl(*(uint32_t*)(buf + off));
    off += sizeof(uint32_t);
    msg->seq_ack = ntohl(*(uint32_t*)(buf + off));
    off += sizeof(uint32_t);
    msg->mtu = ntohl(*(uint32_t*)(buf + off));
    off += sizeof(uint32_t);
    msg->windowsz = ntohl(*(uint32_t*)(buf + off));
    off += sizeof(uint32_t);

    return off;
}

static
int _rdt_decode_handshake_fin_msg(char* buf, int length, struct rdt_common_msg* cmsg)
{
    struct rdt_handshake_fin_msg* msg = (struct rdt_handshake_fin_msg*)cmsg;
    int off = 0;

    vassert(buf);
    vassert(length >= sizeof(*msg));
    vassert(msg);

    off += sizeof(uint8_t);
    off += sizeof(uint8_t);//padx
    msg->rteid = ntohs(*(uint16_t*)(buf + off));
    off += sizeof(uint16_t);

    //pkt->version = ntohs(*(uint16_t*)(buf + off) >> 1);
    off += sizeof(uint16_t);
    off += sizeof(uint16_t);

    msg->seq = ntohl(*(uint32_t*)(buf + off));
    off += sizeof(uint32_t);
    msg->seq_ack = ntohl(*(uint32_t*)(buf + off));
    off += sizeof(uint32_t);

    return off;
}

struct rdt_dec_ops rdt_dec_ops = {
    .data          = _rdt_decode_data_msg,
    .data_ack      = _rdt_decode_data_ack_msg,
    .keepalive     = _rdt_decode_keepalive_msg,
    .shutdown      = _rdt_decode_shutdown_msg,
    .handshake_req = _rdt_decode_handshake_req_msg,
    .handshake_rsp = _rdt_decode_handshake_rsp_msg,
    .handshake_fin = _rdt_decode_handshake_fin_msg
};

