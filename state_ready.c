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
#include "operators.h"

struct rdt_proto_ops state_ready_ops = {
    .handshake_req  = NULL,
    .handshake_resp  = NULL,
    .handshake_fin  = NULL,
    .handshake_delayed_fin = NULL,

    .send_data      = _transfer_send_data,
    .send_data_ack  = _transfer_send_data_ack,
    .send_data_fin  = _transfer_send_data_fin,

    .shutdown       = _shutdown_tunnel,
    .shutdown_recv   = _shutdown_tunnel_recv,

    .keepalive      = _transfer_keepalive,
    .keepalive_recv = _transfer_keepalive_recv,
};
