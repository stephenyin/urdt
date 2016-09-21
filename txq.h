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

#ifndef __TXQ_H__
#define __TXQ_H__

#include "codec.h"
#include "varray.h"
#include "vsys.h"

#define MAX_TXQ_LEN 1024
#define RESEND_TRIGGER_COUNT 3

typedef struct tx_pkt_mngr{
    struct vlock lock;
    struct vlock tx_lock;
    struct vcond tx_cond;
    struct varray* pkt_list;

    int32_t max_pkt_num;
    int32_t send_index;
    uint32_t last_ack;
    int32_t ack_counter;
    void (*init)(void* this);
    void (*deinit)(void* this);
    int32_t (*push_pkt)(void* this, data_encoded_pkt_t* pkt);
    int32_t (*update_ack)(void* this, int seq_ack);
    int32_t (*fetch_pkt)(void* this, data_encoded_pkt_t** ppkt);
    int32_t (*trigger_resend)(void* this);
} tx_pkt_mngr_t;

void init_txq(void* this);
void deinit_txq(void* this);
int32_t push_pkt(void* this, data_encoded_pkt_t* pkt);
int32_t update_ack(void* this, int seq_ack);
int32_t fetch_txq_pkt(void* this, data_encoded_pkt_t** ppkt);
int32_t trigger_resend(void* this);

#endif
