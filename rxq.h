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

#ifndef __RXQ_H__
#define __RXQ_H__

#include "codec.h"
#include "vlist.h"
#include "vsys.h"

#define MAX_RXQ_LEN 255

typedef struct rx_pkt_mngr{
    struct vlock lock;
    struct vlock rx_lock;
    struct vcond rx_cond;
    struct vlist pkt_list;
    struct vlist commit_list;

    uint32_t max_pkt_num;
    uint32_t cur_pkt_num;
    uint32_t expected_seq;

    void (*init)(void* this);
    void (*deinit)(void* this);
    uint32_t (*arrange_pkt)(void* this, data_pkt_t* pkt);
    int32_t (*fetch_pkt)(void* this, data_pkt_t** ppkt);
} rx_pkt_mngr_t;

void init_rxq(void* this);
void deinit_rxq(void* this);
uint32_t arrange_pkt(void* this, data_pkt_t* pkt);
int32_t fetch_rxq_pkt(void* this, data_pkt_t** ppkt);

#endif
