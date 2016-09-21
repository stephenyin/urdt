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
#include "codec.h"
#include "headers.h"
#include "operators.h"

struct rdt_proto_ops state_closed_ops = {
    .handshake_req  = _handshake_request,
    .handshake_resp  = _handshake_response,
    .handshake_fin  = NULL,
    .handshake_delayed_fin = NULL,

    .send_data      = NULL,
    .send_data_ack  = NULL,
    .send_data_fin  = NULL,

    .shutdown       = NULL,
    .shutdown_recv   = NULL,

    .keepalive      = NULL,
    .keepalive_recv = NULL,
};
