/*
 * ipset.c - skeleton vpp engine plug-in
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

#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <ipset/ipset.h>
#include <vnet/devices/netlink.h>

#include <vlibapi/api.h>
#include <vlibmemory/api.h>
#include <vpp/app/version.h>
#include <stdbool.h>
#include <vnet/fib/fib_table.h>

#include <ipset/ipset.api_enum.h>
#include <ipset/ipset.api_types.h>

#define REPLY_MSG_ID_BASE imp->msg_id_base
#include <vlibapi/api_helper_macros.h>

ipset_main_t ipset_main;
static char *ifname = "nlmon0";
static char *iftype = "nlmon";

/* Action function shared between message handler and debug CLI */

void
ipset_route_add (ipset_main_t *imp, fib_prefix_t *prefix, u32 sw_if_index)
{
  ip46_address_t nh;
  nh.ip4.data[0] = 192;
  nh.ip4.data[1] = 168;
  nh.ip4.data[2] = 1;
  nh.ip4.data[3] = 2;
  fib_table_entry_path_add (
    fib_table_get_index_for_sw_if_index (FIB_PROTOCOL_IP4, sw_if_index),
    prefix, imp->fib_src, FIB_ENTRY_FLAG_NONE, DPO_PROTO_IP4, &nh, sw_if_index,
    ~0, 1, NULL, FIB_ROUTE_PATH_FLAG_NONE);
}

static clib_error_t *
ipset_enable_disable (ipset_main_t *imp, int enable_disable)
{
  clib_error_t *err = NULL;
  u8 *ifname_to_ipset = NULL;
  if (enable_disable)
    {
      // netlink
      err = vnet_netlink_add_link (ifname, iftype);
      if (err)
	{
	  goto error;
	}
      // af packet
      vec_validate_init_c_string (ifname_to_ipset, ifname, strlen (ifname));
      int rv = imp->af_packet_vft_table.af_packet_create_if (
	ifname_to_ipset, &imp->af_packet_sw_index);
      if (rv == VNET_API_ERROR_SYSCALL_ERROR_1)
	{
	  err =
	    clib_error_return (0, "%s (errno %d)", strerror (errno), errno);
	  goto error;
	}

      if (rv == VNET_API_ERROR_INVALID_INTERFACE)
	{
	  err = clib_error_return (0, "Invalid interface name");
	  goto error;
	}

      if (rv == VNET_API_ERROR_SUBIF_ALREADY_EXISTS)
	{
	  err = clib_error_return (0, "Interface already exists");
	  goto error;
	}
      vnet_sw_interface_admin_up (imp->vnet_main, imp->af_packet_sw_index);
      vnet_feature_enable_disable ("device-input", "ipset-input",
				   imp->af_packet_sw_index, 1, 0, 0);
    }
  else
    {
      err = vnet_netlink_del_link (ifname);
      if (err)
	{
	  goto error;
	}
      vec_validate_init_c_string (ifname_to_ipset, ifname, strlen (ifname));
      imp->af_packet_vft_table.af_packet_delete_if (ifname_to_ipset);
    }

  imp->enabled = enable_disable;
error:
  if (ifname_to_ipset)
    vec_free (ifname_to_ipset);
  return err;
}

static clib_error_t *
ipset_route_add_command_fn (vlib_main_t *vm, unformat_input_t *input,
			    vlib_cli_command_t *cmd)
{
  ipset_main_t *imp = &ipset_main;
  u32 sw_if_index = ~0;
  fib_prefix_t alidns = {
    .fp_len = 32,
    .fp_proto = FIB_PROTOCOL_IP4,
  };
  alidns.fp_addr.ip4.data[0] = 223;
  alidns.fp_addr.ip4.data[1] = 5;
  alidns.fp_addr.ip4.data[2] = 5;
  alidns.fp_addr.ip4.data[3] = 5;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "intfc %U", unformat_vnet_sw_interface,
		    imp->vnet_main, &sw_if_index))
	;
      else
	break;
    }

  ipset_route_add (imp, &alidns, sw_if_index);

  return 0;
}

static clib_error_t *
ipset_enable_disable_command_fn (vlib_main_t *vm, unformat_input_t *input,
				 vlib_cli_command_t *cmd)
{
  ipset_main_t *imp = &ipset_main;
  int enable_disable = 1;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "disable"))
	enable_disable = 0;
      else
	break;
    }

  return ipset_enable_disable (imp, enable_disable);
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (ipset_enable_disable_command, static) = {
  .path = "ipset enable-disable",
  .short_help = "ipset enable-disable [disable]",
  .function = ipset_enable_disable_command_fn,
};
VLIB_CLI_COMMAND (ipset_test_add_route_command, static) = {
  .path = "ipset route add",
  .short_help = "ipset route add sw-if-index <sw-if-index>",
  .function = ipset_route_add_command_fn,
};
/* *INDENT-ON* */

/* API message handler */
static void
vl_api_ipset_enable_disable_t_handler (vl_api_ipset_enable_disable_t *mp)
{
  vl_api_ipset_enable_disable_reply_t *rmp;
  ipset_main_t *imp = &ipset_main;
  int rv = 0;

  ipset_enable_disable (imp, (int) (mp->enable_disable));

  REPLY_MACRO (VL_API_IPSET_ENABLE_DISABLE_REPLY);
}

/* API definitions */
#include <ipset/ipset.api.c>

static clib_error_t *
ipset_init (vlib_main_t *vm)
{
  ipset_main_t *imp = &ipset_main;
  clib_error_t *error = 0;

  imp->vlib_main = vm;
  imp->vnet_main = vnet_get_main ();

  /* Add our API messages to the global name_crc hash table */
  imp->msg_id_base = setup_message_id_table ();

  /* Init af packet */
  imp->af_packet_vft_table.af_packet_create_if = vlib_get_plugin_symbol (
    "af_packet_plugin.so", "af_packet_create_if_simple");
  imp->af_packet_vft_table.af_packet_delete_if =
    vlib_get_plugin_symbol ("af_packet_plugin.so", "af_packet_delete_if");
  imp->af_packet_sw_index = ~0;

  /* Init ip fib */
  imp->fib_src = fib_source_allocate ("ipset", FIB_SOURCE_PRIORITY_LOW,
				      FIB_SOURCE_BH_SIMPLE);

  return error;
}

VLIB_INIT_FUNCTION (ipset_init);

/* *INDENT-OFF* */
VNET_FEATURE_INIT (ipset, static) = {
  .arc_name = "device-input",
  .node_name = "ipset-input",
};
/* *INDENT-ON */

/* *INDENT-OFF* */
VLIB_PLUGIN_REGISTER () = {
  .version = VPP_BUILD_VER,
  .description = "ipset",
};
/* *INDENT-ON* */

/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
