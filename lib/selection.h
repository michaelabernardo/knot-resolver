/*  Copyright (C) 2014-2020 CZ.NIC, z.s.p.o. <knot-dns@labs.nic.cz>
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

/**
 * @file selection.h
 * Provides server selection API (see `kr_server_selection`) and functions common to both implementations.
 */

#include "lib/cache/api.h"

/**
 * These errors are to be reported as feedback to server selection.
 * See `kr_server_selection::error` for more details.
 */
enum kr_selection_error {
	// Network errors
	KR_SELECTION_TIMEOUT,
	KR_SELECTION_TLS_HANDSHAKE_FAILED,
	KR_SELECTION_TCP_CONNECT_FAILED,
	KR_SELECTION_TCP_CONNECT_TIMEOUT,

	// RCODEs
	KR_SELECTION_REFUSED,
	KR_SELECTION_SERVFAIL,
	KR_SELECTION_FORMERROR,
	KR_SELECTION_NOTIMPL,
	KR_SELECTION_OTHER_RCODE,
	KR_SELECTION_TRUNCATED,

	// DNS errors
	KR_SELECTION_DNSSEC_ERROR,
	KR_SELECTION_LAME_DELEGATION,

	KR_SELECTION_NUMBER_OF_ERRORS /**< Leave this last, as it is use as array size. */
};

enum kr_transport_protocol {
	KR_TRANSPORT_NOADDR = 0, /**< Selected name with no address, it has to be resolved first.*/
	KR_TRANSPORT_UDP,
	KR_TRANSPORT_TCP,
	KR_TRANSPORT_TLS,
};


/**
 * Output of the selection algorithm.
 */
struct kr_transport {
	knot_dname_t *name; /**< Set to "." for forwarding targets.*/
	union inaddr address;
	size_t address_len;
	enum kr_transport_protocol protocol;
	unsigned timeout; /**< Timeout in ms to be set for UDP transmission. */
	bool deduplicated; /**< True iff transport was set in worker.c:subreq_finalize,
                                that means it may be different from the one originally chosen one.*/
};

/**
 * Specifies a API for selecting transports and giving feedback on the choices.
 *
 * The function pointers are to be used throughout resolver when some information about
 * the transport is obtained. E.g. RTT in `worker.c` or RCODE in `iterate.c`,…
 */
struct kr_server_selection
{
	bool initialized;
	/**
	 * Puts a pointer to next transport of @p qry to @p transport .
	 *
	 * Allocates new kr_transport, chooses transport to be used for this query.
	 * Selection may fail, so @p transport can be set to NULL.
	 *
	 * @param transport to be filled with pointer to the chosen transport or NULL on failure
	 */
	void (*choose_transport)(struct kr_query *qry, struct kr_transport **transport);
	/// To be called when all went OK
	void (*success)(struct kr_query *qry, const struct kr_transport *transport);
	/// Report back the RTT of network operation for transport in ms.
	void (*update_rtt)(struct kr_query *qry, const struct kr_transport *transport, unsigned rtt);
	/// Report back error encourtered with the chosen transport. See `enum kr_selection`
	void (*error)(struct kr_query *qry, const struct kr_transport *transport, enum kr_selection_error error);

	int timeouts; /**< Number of timeouts that occured resolving this query.*/
	void *local_state; /**< Inner state of the implementation.*/
};

/**
 * Initialize the server selection API for @p qry.
 *
 * The implementation is to be chosen based on qry->flags.
 */
KR_EXPORT
void kr_server_selection_init(struct kr_query *qry);


/**
 * Add forwarding target to request.
 *
 * This is exposed to Lua in order to add forwarding targets to request.
 * These are then shared by all the queries in said request.
 */
KR_EXPORT
int kr_forward_add_target(struct kr_request *req, size_t index, const struct sockaddr *sock);

/**
 * To be held per IP address in the global LMDB cache
 */
struct rtt_state {
	int32_t srtt;
	int32_t variance;
	int32_t consecutive_timeouts;
};

/**
 * To be held per IP address and locally "inside" query.
 */
struct address_state {
	unsigned int generation;
	struct rtt_state rtt_state;
	knot_dname_t *name;
	bool tls_capable : 1;
	bool tcp_waiting : 1;
	bool tcp_connected : 1;

	int forward_index;
	int error_count;
	int errors[KR_SELECTION_NUMBER_OF_ERRORS];
};

/**
 * Array of these is one of inputs for the actual selection algorithm (`choose_transport`)
 */
struct choice {
	uint8_t *address;
	size_t address_len;
	struct address_state *address_state;
	uint16_t port; /**< used to overwrite the port number; if zero, `choose_transport` determines it*/
};

struct kr_transport *choose_transport(struct choice choices[], int choices_len,
                                      knot_dname_t *unresolved[], int unresolved_len,
                                      int timeouts, struct knot_mm *mempool, bool tcp,
				      size_t *out_forward_index);
void update_rtt(struct kr_query *qry, struct address_state *addr_state,
                const struct kr_transport *transport, unsigned rtt);
void error(struct kr_query *qry, struct address_state *addr_state,
           const struct kr_transport *transport, enum kr_selection_error sel_error);

struct rtt_state get_rtt_state(const uint8_t *ip, size_t len, struct kr_cache *cache);
int put_rtt_state(const uint8_t *ip, size_t len, struct rtt_state state, struct kr_cache *cache);

void bytes_to_ip(uint8_t *bytes, size_t len, union inaddr *dst);
uint8_t* ip_to_bytes(const union inaddr *src, size_t len);

void check_tls_capable(struct address_state *address_state, struct kr_request *req,
                       struct sockaddr *address);
void check_tcp_connections(struct address_state *address_state, struct kr_request *req,
                           struct sockaddr *address);
void check_network_settings(struct address_state *address_state, size_t address_len,
                            bool no_ipv4, bool no_ipv6);


