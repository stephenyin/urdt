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
#include "rxq.h"

static struct vlist* find_position(rx_pkt_mngr_t* pkt_mngr, data_pkt_t* pkt);
static uint32_t get_expected_req(rx_pkt_mngr_t* pkt_mngr, data_pkt_t* last_accepted_pkt);
static uint32_t commit_pkt(rx_pkt_mngr_t* pkt_mngr);
static int next_pkt(struct vlist*, void*);

void init_rxq(void* this)
{
    vlogD("RXQ:init_rxq");

    vassert(this != NULL);

    rx_pkt_mngr_t* pkt_mngr = (rx_pkt_mngr_t*) this;
    vlock_init(&pkt_mngr->lock);

    vlock_init(&pkt_mngr->rx_lock);
    vcond_init(&pkt_mngr->rx_cond);
    vlist_init(&pkt_mngr->pkt_list);
    vlist_init(&pkt_mngr->commit_list);

    pkt_mngr->max_pkt_num = MAX_RXQ_LEN;
    pkt_mngr->cur_pkt_num = 0;
    pkt_mngr->expected_seq = 1;
}

void deinit_rxq(void* this)
{
    vlogD("RXQ:Deinit_rxq");

    vassert(this != NULL);
    rx_pkt_mngr_t* pkt_mngr = (rx_pkt_mngr_t*) this;

    vlock_deinit(&pkt_mngr->lock);
    vlock_deinit(&pkt_mngr->rx_lock);
    vcond_deinit(&pkt_mngr->rx_cond);
}

uint32_t arrange_pkt(void* this, data_pkt_t* pkt)
{
    vassert(this != NULL);
    vassert(pkt != NULL);

    //vlogD("RXQ:arrange_pkt (seq:%d)", pkt->seq);

    rx_pkt_mngr_t* pkt_mngr = (rx_pkt_mngr_t*) this;

    if(pkt->seq < pkt_mngr->expected_seq){
        return pkt_mngr->expected_seq;
    }

    struct vlist* node = NULL;

    vlock_enter(&pkt_mngr->lock);
    node = find_position(pkt_mngr, pkt);
    if(node != NULL){
        vlist_add_tail(node, &pkt->list);
        //vlogE("req(%d) expected_seq(%u)", pkt->seq, pkt_mngr->expected_seq);
        if(pkt->seq == pkt_mngr->expected_seq) {
            pkt_mngr->expected_seq = get_expected_req(pkt_mngr, pkt);
            commit_pkt(pkt_mngr);
            vcond_signal(&pkt_mngr->rx_cond);
        }
        pkt_mngr->cur_pkt_num++;
    }

    vlock_leave(&pkt_mngr->lock);

    //vlogE("expected_seq(%d)", pkt_mngr->expected_seq);

    return pkt_mngr->expected_seq;
}

struct vlist* find_position(rx_pkt_mngr_t* pkt_mngr, data_pkt_t* pkt)
{
    vassert(pkt != NULL);
    vassert(pkt_mngr != NULL);

    //vlogD("RXQ:find_position (seq:%d)", pkt->seq);

    struct vlist*  node = NULL;
    data_pkt_t* member = NULL;

    if(vlist_is_empty(&pkt_mngr->pkt_list)){
        return &pkt_mngr->pkt_list;
    }

    __vlist_for_each(node, &pkt_mngr->pkt_list) {
        member = vlist_entry(node, struct data_pkt, list);
        if (member->seq == pkt->seq) {
            vlogD("The pkt with same req(%u) exists in RXQ. ignore this one!!", member->seq);
            //The pkt with same req exists in RXQ. ignore this one.
            return NULL;
        } else if (member->seq > pkt->seq) {
            //vlogE("Fill in %u", pkt->seq);
            return &member->list;
        } else {
            continue;
        }
    }

    return &pkt_mngr->pkt_list;
}

int32_t fetch_rxq_pkt(
void* this, data_pkt_t** ppkt)
{
    //vlogD("RXQ:fetch_rxq_pkt ");

    vassert(this != NULL);
    vassert(ppkt != NULL);

    int ret = 0;
    rx_pkt_mngr_t* pkt_mngr = (rx_pkt_mngr_t*) this;

    struct vlist*   node = NULL;

    vlock_enter(&pkt_mngr->rx_lock);
    node = vlist_pop_head(&pkt_mngr->commit_list);
    ret = !vlist_is_empty(&pkt_mngr->commit_list);
    vlock_leave(&pkt_mngr->rx_lock);

    if(node == NULL) {
        //vlogD("RXQ:Nothing to be fetched!!");
        return -1;
    }

    *ppkt = vlist_entry(node, data_pkt_t, list);

    return ret;
}


int next_pkt(struct vlist* node, void* argv)
{
    vassert(argv != NULL);

    data_pkt_t* pkt = (data_pkt_t*) argv;
    data_pkt_t* pkt_in_list = vlist_entry(node, data_pkt_t, list);
    if(pkt_in_list == NULL) {
        return 0;
    }

    return (pkt_in_list->seq == pkt->seq + pkt->len);
}

uint32_t get_expected_req(rx_pkt_mngr_t* pkt_mngr, data_pkt_t* last_accepted_pkt)
{
    //vlogD("RXQ:get_expected_req");

    vassert(pkt_mngr != NULL);
    vassert(last_accepted_pkt != NULL);

    struct vlist*  node = NULL;
    data_pkt_t* pkt = NULL;

    if(vlist_is_empty(&pkt_mngr->pkt_list)){
        return last_accepted_pkt->seq + last_accepted_pkt->len;
    }

    pkt = last_accepted_pkt;
    do{
        node = vlist_find_node(&pkt_mngr->pkt_list, &next_pkt,(void*)pkt);
        if(node == NULL) {
            return pkt->seq + pkt->len;
        }

        pkt = vlist_entry(node, data_pkt_t, list);

    } while(pkt != NULL);

    //TODO log
    vlogE("RXQ:get_expected_req failed");
    vassert(0);

    return 0;
}

uint32_t commit_pkt(rx_pkt_mngr_t* pkt_mngr)
{
    vassert(pkt_mngr != NULL);

    struct vlist*   node = NULL;
    data_pkt_t*  pkt = NULL;
    data_pkt_t*  next_pkt = NULL;
    uint32_t counter = 0;

    while(1){
        node = vlist_pop_head(&pkt_mngr->pkt_list);
        if(node == NULL) {
            vlogD("RXQ:vlist_pop_head NULL ");
            break;
        }

        vlock_enter(&pkt_mngr->rx_lock);
        vlist_add_tail(&pkt_mngr->commit_list, node);
        vlock_leave(&pkt_mngr->rx_lock);
        counter++;

        if(vlist_is_empty(&pkt_mngr->pkt_list)){
            //No next pkt exists
            break;
        }

        pkt = vlist_entry(node, data_pkt_t, list);
        next_pkt = vlist_entry(pkt_mngr->pkt_list.next, data_pkt_t, list);
        if(next_pkt->seq != pkt->seq + pkt->len){
            //Next pkt is not sequential
            break;
        }
    }

    return counter;
}

