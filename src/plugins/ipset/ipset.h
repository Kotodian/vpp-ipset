
/*
 * ipset.h - skeleton vpp engine plug-in header file
 *
 * Copyright (c) <current-year> <your-organization>
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __included_ipset_h__
#define __included_ipset_h__

#include <vnet/vnet.h>
#include <vnet/ip/ip.h>
#include <vnet/fib/fib_source.h>
#include <vnet/ethernet/ethernet.h>

#include <vppinfra/hash.h>
#include <vppinfra/error.h>

typedef struct af_packet_vft_
{
  /** Create af packet interface **/
  int (*af_packet_create_if) (u8 *host_if_name, u32 *sw_if_index);
  /** Delete af packet interface **/
  void (*af_packet_delete_if) (u8 *host_if_name);
} af_packet_vft;

typedef struct
{
  /* API message ID base */
  u16 msg_id_base;

  /* on/off switch for the periodic function */
  u8 periodic_timer_enabled;
  /* Node index, non-zero if the periodic process has been created */
  u32 periodic_node_index;
  /* is ipset main enabled */
  bool enabled;

  /* af packet plugin */
  af_packet_vft af_packet_vft_table;
  u32 af_packet_sw_index;

  /* ip fib */
  fib_source_t fib_src;
  u32 fib_sync_node_index;

  /* convenience */
  vlib_main_t *vlib_main;
  vnet_main_t *vnet_main;
  ethernet_main_t *ethernet_main;
} ipset_main_t;

extern ipset_main_t ipset_main;

extern vlib_node_registration_t ipset_periodic_node;

/* Periodic function events */
#define IPSET_EVENT1			    1
#define IPSET_EVENT2			    2
#define IPSET_EVENT_PERIODIC_ENABLE_DISABLE 3

#define IPSET_FIB_ADD_EVENT    1
#define IPSET_FIB_DELETE_EVENT 2

void ipset_create_periodic_process (ipset_main_t *);
void ipset_route_add (ipset_main_t *imp, fib_prefix_t *prefix,
		      u32 sw_if_index);

#endif /* __included_ipset_h__ */

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
