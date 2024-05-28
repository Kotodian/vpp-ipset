/*
 * node.c - skeleton vpp engine plug-in dual-loop node skeleton
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
#include <libmnl/libmnl.h>
#include <linux/netlink.h>
#include <vlib/vlib.h>
#include <vnet/vnet.h>
#include <vnet/pg/pg.h>
#include <vppinfra/error.h>
#include <ipset/ipset.h>
#include <libipset/mnl.h>

static vlib_node_registration_t ipset_node;

typedef struct
{
  struct nlmsghdr nlmsg_hdr;
  ip_prefix_t prefix;
} ipset_trace_t;

/* packet trace format function */
static u8 *
format_ipset_trace (u8 *s, va_list *args)
{
  CLIB_UNUSED (vlib_main_t * vm) = va_arg (*args, vlib_main_t *);
  CLIB_UNUSED (vlib_node_t * node) = va_arg (*args, vlib_node_t *);
  ipset_trace_t *t = va_arg (*args, ipset_trace_t *);
  u32 indent = format_get_indent (s);

  s = format (s,
	      "%Unlmsg_hdr:\n%Utotal_len %u message_type "
	      "%u message_flags %u sequence_number %u "
	      "%U route",
	      format_white_space, indent + 2, format_white_space, indent + 4,
	      t->nlmsg_hdr.nlmsg_len, t->nlmsg_hdr.nlmsg_type,
	      t->nlmsg_hdr.nlmsg_flags, t->nlmsg_hdr.nlmsg_seq,
	      format_ip_prefix, &t->prefix);

  return s;
}

#define foreach_ipset_error                                                   \
  _ (ADD_INFO, "Recieve IPSET CMD ADD")                                       \
  _ (DEL_INFO, "Recieve IPSET CMD DEL")

typedef enum
{
#define _(sym, str) IPSET_ERROR_##sym,
  foreach_ipset_error
#undef _
    IPSET_N_ERROR,
} ipset_error_t;

static char *ipset_error_strings[] = {
#define _(sym, string) string,
  foreach_ipset_error
};

typedef enum
{
  IPSET_NEXT_DROP,
  IPSET_N_NEXT,
} ipset_next_t;

#define foreach_mac_address_offset                                            \
  _ (0)                                                                       \
  _ (1)                                                                       \
  _ (2)                                                                       \
  _ (3)                                                                       \
  _ (4)                                                                       \
  _ (5)
static int
ipset_nl_nested_attr_ip_cb (const struct nlattr *nested, void *data)
{
  ip_prefix_t *prefix = (ip_prefix_t *) data;

  ip4_address_t ip4;
  ip4.as_u32 = mnl_attr_get_u32 (nested);
  ip46_address_t ip46 = to_ip46 (0, (u8 *) (&ip4));
  ip_address_from_46 (&ip46, FIB_PROTOCOL_IP4, &prefix->addr);

  return 0;
}

always_inline int
ipset_nl_nested_attr_cidr (const struct nlattr *nested, void *data)
{
  ip_prefix_t *prefix = (ip_prefix_t *) data;
  prefix->len = mnl_attr_get_u32 (nested);

  return 0;
}

VLIB_NODE_FN (ipset_node)
(vlib_main_t *vm, vlib_node_runtime_t *node, vlib_frame_t *frame)
{
  u32 n_left_from, *from, *to_next;
  ipset_next_t next_index;

  from = vlib_frame_vector_args (frame);
  n_left_from = frame->n_vectors;
  next_index = node->cached_next_index;
  u32 pkts_cmd_add = 0, pkts_cmd_del = 0;

  while (n_left_from > 0)
    {
      u32 n_left_to_next;

      vlib_get_next_frame (vm, node, next_index, to_next, n_left_to_next);

      while (n_left_from > 0 && n_left_to_next > 0)
	{
	  u32 bi0;
	  vlib_buffer_t *b0;
	  u32 next0 = IPSET_NEXT_DROP;
	  struct nlmsghdr *nlh0;
	  int ipset_cmd_type0;
	  ip_prefix_t route;

	  clib_memset (&route, 0, sizeof (ip_prefix_t));
	  /* speculatively enqueue b0 to the current next frame */
	  bi0 = from[0];
	  to_next[0] = bi0;
	  from += 1;
	  to_next += 1;
	  n_left_from -= 1;
	  n_left_to_next -= 1;

	  b0 = vlib_get_buffer (vm, bi0);

	  nlh0 = vlib_buffer_get_current (b0);
	  switch (nlh0->nlmsg_type)
	    {
	    case NLMSG_NOOP:
	    case NLMSG_DONE:
	    case NLMSG_OVERRUN:
	    case NLMSG_ERROR:
	      goto next_msg;
	    default:;
	    }
	  // filter ipset message type
	  ipset_cmd_type0 = ipset_get_nlmsg_type (nlh0);
	  // only add and del
	  if (ipset_cmd_type0 == IPSET_CMD_ADD)
	    {

	      int len = 0, offset = 0;
	      struct nlattr *attr;

	      offset = NLMSG_HDRLEN + NLMSG_ALIGN (sizeof (struct nfgenmsg));
	      attr = (struct nlattr *) ((u8 *) nlh0 + offset);
	      len = nlh0->nlmsg_len - offset;

	      do
		{
		  if (attr->nla_type & NLA_F_NESTED)
		    {
		      struct nlattr *nested;

		      mnl_attr_for_each_nested (nested, attr)
		      {
			if ((nested->nla_type & NLA_TYPE_MASK) ==
			      IPSET_ATTR_IP_FROM &&
			    nested->nla_type & NLA_F_NESTED)
			  {
			    mnl_attr_parse_nested (
			      nested, ipset_nl_nested_attr_ip_cb, &route);
			  }
			else if ((nested->nla_type & NLA_TYPE_MASK) ==
				 IPSET_ATTR_CIDR)
			  {
			    ipset_nl_nested_attr_cidr (nested, &route);
			  }
		      }
		    }
		  offset = NLA_ALIGN (attr->nla_len);
		  len -= offset;
		  attr = (struct nlattr *) ((u8 *) attr + offset);
		}
	      while (len > sizeof (struct nlattr));
	      pkts_cmd_add++;
	    }
	  else if (ipset_cmd_type0 == IPSET_CMD_DEL)
	    {
	      int len = 0, offset = 0;
	      struct nlattr *attr;

	      offset = NLMSG_HDRLEN + NLMSG_ALIGN (sizeof (struct nfgenmsg));
	      attr = (struct nlattr *) ((u8 *) nlh0 + offset);
	      len = nlh0->nlmsg_len - offset;

	      do
		{
		  if (attr->nla_type & NLA_F_NESTED)
		    {
		      struct nlattr *nested;

		      mnl_attr_for_each_nested (nested, attr)
		      {
			if ((nested->nla_type & NLA_TYPE_MASK) ==
			      IPSET_ATTR_IP_FROM &&
			    nested->nla_type & NLA_F_NESTED)
			  {
			    mnl_attr_parse_nested (
			      nested, ipset_nl_nested_attr_ip_cb, &route);
			  }
			else if ((nested->nla_type & NLA_TYPE_MASK) ==
				 IPSET_ATTR_CIDR)
			  {
			    ipset_nl_nested_attr_cidr (nested, &route);
			  }
		      }
		    }
		  offset = NLA_ALIGN (attr->nla_len);
		  len -= offset;
		  attr = (struct nlattr *) ((u8 *) attr + offset);
		}
	      while (len > sizeof (struct nlattr));
	      pkts_cmd_del++;
	    }

	next_msg:
	  if (PREDICT_FALSE ((node->flags & VLIB_NODE_FLAG_TRACE) &&
			     (b0->flags & VLIB_BUFFER_IS_TRACED)))
	    {
	      ipset_trace_t *t = vlib_add_trace (vm, node, b0, sizeof (*t));
	      clib_memset (&t->nlmsg_hdr, 0, sizeof (struct nlmsghdr));
	      clib_memcpy (&t->nlmsg_hdr, nlh0, sizeof (struct nlmsghdr));
	      t->prefix = route;
	    }

	  /* verify speculative enqueue, maybe switch current next frame */
	  vlib_validate_buffer_enqueue_x1 (vm, node, next_index, to_next,
					   n_left_to_next, bi0, next0);
	}

      vlib_put_next_frame (vm, node, next_index, n_left_to_next);
    }

  vlib_node_increment_counter (vm, ipset_node.index, IPSET_ERROR_ADD_INFO,
			       pkts_cmd_add);
  vlib_node_increment_counter (vm, ipset_node.index, IPSET_ERROR_DEL_INFO,
			       pkts_cmd_del);
  return frame->n_vectors;
}

/* *INDENT-OFF* */
VLIB_REGISTER_NODE (ipset_node, static) = 
{
  .name = "ipset-input",
  .vector_size = sizeof (u32),
  .format_trace = format_ipset_trace,
  .type = VLIB_NODE_TYPE_INTERNAL,
  
  .n_errors = ARRAY_LEN(ipset_error_strings),
  .error_strings = ipset_error_strings,

  .n_next_nodes = IPSET_N_NEXT,

  .next_nodes = {
        [IPSET_NEXT_DROP] = "drop",
  },
};
/* *INDENT-ON* */
/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
