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

#include "ecRdt.h"
#include "tunnel.h"
#include "receiver.h"
#include "vassert.h"

ecRdtInitializer g_rdtOpendCallback = {.onRdtOpened = NULL};
uint8_t g_rdtInitialized = 0;

extern int session_set_cb(void (*cb)(int, int, void*, int), int type);    //rdt: type = 0

int ecRdtModuleInitialize(ecRdtInitializer* initializer)
{
    int err = ECRDT_E_BAD_PARAM;

    retE((!initializer), err);
    retE((!initializer->onRdtOpened), err);
    retE((g_rdtInitialized), ECRDT_E_ALREADY_STARTED);

    g_rdtOpendCallback.onRdtOpened = initializer->onRdtOpened;
    session_set_cb(on_session_data, 0);
    g_rdtInitialized = 1;

    return 0;
}

int ecRdtModuleDestroy(void)
{
    if(g_rdtInitialized == 0){
        vlogE("Rdt module has not been Initialized!");
        return 0;
    }

    g_rdtOpendCallback.onRdtOpened = NULL;
    destroy_all_tunnel();
    g_rdtInitialized = 0;
    return 0;
}

int ecRdtOpen(int sessionId, int channelId, ecRdtHandler* handler)
{
    int err = ECRDT_E_BAD_PARAM;
    rdt_tunnel_t* tunnel = NULL;
    int ret = 0;

    retE((sessionId <= 0), err);
    retE((channelId <= 0), err);
    retE((!handler), err);
    retE((!handler->onClosed), err);
    retE((!handler->onData), err);
    retE((!g_rdtInitialized), ECRDT_E_NOT_STARTED);

    ret = create_tunnel(sessionId, channelId, handler, &tunnel);
    if (ret < 0) {
        vlogE("Create tunnel failed");
        return -1;
    }

    return tunnel->teid;
}

int ecRdtClose(int rdtId)
{
    rdt_tunnel_t* tunnel = NULL;

    retE((rdtId <= 0), ECRDT_E_BAD_PARAM);
    retE((!g_rdtInitialized), ECRDT_E_NOT_STARTED);

    tunnel = get_tunnel(rdtId);
    if (!tunnel) {
        vlogE("No such tunnel exists");
        return ECRDT_E_BAD_RDT_TUNNEL;
    }

    destroy_tunnel(tunnel, 1);
    return 0;
}

int ecRdtWrite(int rdtId, const void* data, int length)
{
    int err = ECRDT_E_BAD_PARAM;
    rdt_tunnel_t* tunnel = NULL;

    retE((rdtId < 0), err);
    retE((!data), err);
    retE((length <= 0), err);
    retE((!g_rdtInitialized), ECRDT_E_NOT_STARTED);

    tunnel = get_tunnel(rdtId);
    if (!tunnel) {
        vlogE("No such tunnel exists");
        return ECRDT_E_BAD_RDT_TUNNEL;
    }

    return tunnel_send_data(tunnel, data, length);
}

int ecRdtGetInfo(int rdtId, ecRdtInfo* info)
{
    int err = ECRDT_E_BAD_PARAM;
    rdt_tunnel_t* tunnel = NULL;

    retE((rdtId < 0), err);
    retE((!info), err);
    retE((!g_rdtInitialized), ECRDT_E_NOT_STARTED);

    tunnel = get_tunnel(rdtId);
    if (!tunnel) {
        vlogE("No such tunnel exists");
        return ECRDT_E_BAD_RDT_TUNNEL;
    }

    info->channelId = tunnel->channelId;
    info->sessionId = tunnel->sessionId;
    info->bytesOfSent = tunnel->tx_bytes;
    info->bytesOfReceived = tunnel->rx_bytes;
    return 0;
}

