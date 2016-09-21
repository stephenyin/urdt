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

#include "headers.h"
#include "vassert.h"
#include "vsys.h"
#include "txq.h"

static void update_q(tx_pkt_mngr_t* pkt_mngr, uint32_t ack);
static int32_t ack2index(tx_pkt_mngr_t* pkt_mngr, uint32_t ack);

void init_txq(void* this)
{
    vlogD("TXQ:Init txq");

    vassert(this != NULL);
    tx_pkt_mngr_t* pkt_mngr = (tx_pkt_mngr_t*) this;

    vlock_init(&pkt_mngr->lock);
    vlock_init(&pkt_mngr->tx_lock);
    vcond_init(&pkt_mngr->tx_cond);

    pkt_mngr->pkt_list = (struct varray*)malloc(sizeof(struct varray));
    if(pkt_mngr->pkt_list == NULL){
        return;
    }

    varray_init(pkt_mngr->pkt_list, 0);

    pkt_mngr->max_pkt_num = MAX_TXQ_LEN;
    pkt_mngr->send_index = 0;
    pkt_mngr->last_ack = 1;
    pkt_mngr->ack_counter = 0;
}

void deinit_txq(void* this)
{
    vlogD("TXQ:Deinit txq");
    vassert(this != NULL);
    tx_pkt_mngr_t* pkt_mngr = (tx_pkt_mngr_t*) this;

    vlock_deinit(&pkt_mngr->lock);
    vlock_deinit(&pkt_mngr->tx_lock);
    vcond_deinit(&pkt_mngr->tx_cond);
    varray_deinit(pkt_mngr->pkt_list);
    if(pkt_mngr->pkt_list != NULL){
        free(pkt_mngr->pkt_list);
    }

}

int32_t push_pkt(void* this, data_encoded_pkt_t* pkt)
{
    vassert(this != NULL);
    vassert(pkt != NULL);

    //vlogD("TXQ:push_pkt seq(%d)", pkt->seq);

    tx_pkt_mngr_t* pkt_mngr = (tx_pkt_mngr_t*) this;

    vlock_enter(&pkt_mngr->lock);
    varray_add_tail(pkt_mngr->pkt_list, pkt);
    vlock_leave(&pkt_mngr->lock);
    vcond_signal(&pkt_mngr->tx_cond);

    //vlogE("TXQ: size(%d)", varray_size(pkt_mngr->pkt_list));
    usleep(varray_size(pkt_mngr->pkt_list) * varray_size(pkt_mngr->pkt_list) * 10);

    return 0;
}

int32_t update_ack(void* this, int seq_ack)
{
    vassert(this != NULL);

    tx_pkt_mngr_t* pkt_mngr = (tx_pkt_mngr_t*) this;

    vlock_enter(&pkt_mngr->lock);

    if(pkt_mngr->last_ack > seq_ack) {
        vlock_leave(&pkt_mngr->lock);
        //vlogE("TXQ:new ack(%u) smaller than last ack(%u).Ignore it!!", pkt->ack, pkt_mngr->last_ack);
        return -1;
    }

    //Repeat the same ack
    if(pkt_mngr->last_ack == seq_ack) {
        pkt_mngr->ack_counter++;
        if(pkt_mngr->ack_counter >= RESEND_TRIGGER_COUNT){
            vlogD("TXQ:Resend pkt(seq:%d)", seq_ack);
            //We assume that the pkt was lost
            pkt_mngr->ack_counter = 0;
            trigger_resend((void*)pkt_mngr);
        }
    } else {
        pkt_mngr->ack_counter = 0;
        pkt_mngr->last_ack = seq_ack;
        int32_t new_send_index = ack2index(pkt_mngr, seq_ack);
        if(new_send_index != -1) {
            if(pkt_mngr->send_index < new_send_index) {
                pkt_mngr->send_index = new_send_index;
            }
        } else {
            //TODO The pkt was not in q yet. waiting
            //vlogD("TXQ:The pkt(seq:%d) was not in txq yet.", seq_ack);
        }

        update_q(pkt_mngr, seq_ack);
    }
    vlock_leave(&pkt_mngr->lock);

    return 0;
}

int32_t fetch_txq_pkt(
void* this, data_encoded_pkt_t** ppkt)
{
    //vlogD("TXQ:fetch_txq_pkt");

    vassert(this != NULL);
    vassert(ppkt != NULL);
    tx_pkt_mngr_t* pkt_mngr = (tx_pkt_mngr_t*) this;

    vlock_enter(&pkt_mngr->lock);
    int sz = varray_size(pkt_mngr->pkt_list);
    if(sz == 0) {
        vlock_leave(&pkt_mngr->lock);
        *ppkt = NULL;
        return 0;
    }

    if(pkt_mngr->send_index < sz){
        *ppkt = (data_encoded_pkt_t*)varray_get(pkt_mngr->pkt_list, pkt_mngr->send_index);
        //vlogD("TXQ:fetch_txq_pkt(seq:%d)", (*ppkt)->seq);
    } else {
        vlock_leave(&pkt_mngr->lock);
        *ppkt = NULL;
        return 0;
    }
    pkt_mngr->send_index++;

    vlock_leave(&pkt_mngr->lock);

    return 1;
}

void update_q(tx_pkt_mngr_t* pkt_mngr, uint32_t ack)
{
   // vlogD("TXQ:update_q(ack:%d)", ack);

    vassert(pkt_mngr != NULL);

    int i = 0;
    data_encoded_pkt_t* pkt = NULL;
    int sz = varray_size(pkt_mngr->pkt_list);

    for (i = sz - 1; i >=  0; i--) {
        pkt = (data_encoded_pkt_t*)varray_get(pkt_mngr->pkt_list, i);
        if (pkt->seq < ack) {
            varray_del(pkt_mngr->pkt_list, i);
            if(pkt->data != NULL){
                //vlogD("TXQ:remove pkt(seq:%d)", pkt->seq);
                free(pkt->data);
            }

            if(pkt != NULL){
                free(pkt);
            }

            pkt_mngr->send_index--;//Cuz array size decreased
            if(pkt_mngr->send_index < 0){
                pkt_mngr->send_index = 0;
            }
        } else {
            break;
        }
    }

}

int32_t ack2index(tx_pkt_mngr_t* pkt_mngr, uint32_t ack)
{
    vassert(pkt_mngr != NULL);

    int i = 0;
    int found  = 0;
    data_encoded_pkt_t* pkt = NULL;
    int sz = varray_size(pkt_mngr->pkt_list);

    for (; i <  sz; i++) {
        pkt = (data_encoded_pkt_t*)varray_get(pkt_mngr->pkt_list, i);
        if (pkt->seq == ack) {
            found = 1;
            break;
        }
    }

    return found == 1 ? i : -1;
}

int32_t trigger_resend(void* this)
{
    vassert(this != NULL);
    vlogD("TXQ: trigger_resend");

    tx_pkt_mngr_t* pkt_mngr = (tx_pkt_mngr_t*) this;

    if(varray_size(pkt_mngr->pkt_list) == 0){
        vlogD("TXQ: empty Q");
        return -1;
    }

    int32_t new_send_index = ack2index(pkt_mngr, pkt_mngr->last_ack);

    vlogD("TXQ: Last ack(%u)-->index(%d)", pkt_mngr->last_ack, new_send_index);
    if(new_send_index != -1) {
        pkt_mngr->send_index = new_send_index;

        vcond_signal(&pkt_mngr->tx_cond);

        vlogD("!!!TXQ: Resend");
        return 0;
    }

    return -1;
}
