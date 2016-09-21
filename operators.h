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

int32_t _handshake_request(struct rdt_tunnel* ptunnel);
int32_t _handshake_response(struct rdt_tunnel* ptunnel);
int32_t _handshake_finish(struct rdt_tunnel* ptunnel);
int32_t _handshake_delayed_finish(struct rdt_tunnel* ptunnel);

int32_t _transfer_send_data(struct rdt_tunnel* ptunnel, const void* data, int length);
int32_t _transfer_send_data_ack(struct rdt_tunnel* ptunnel, uint32_t ack_num);
int32_t _transfer_send_data_fin(struct rdt_tunnel* ptunnel);
int32_t _transfer_keepalive(struct rdt_tunnel* ptunnel);
int32_t _transfer_keepalive_recv(struct rdt_tunnel* rdt);

int32_t _shutdown_tunnel(struct rdt_tunnel* ptunnel);
int32_t _shutdown_tunnel_recv(struct rdt_tunnel* ptunnel);


