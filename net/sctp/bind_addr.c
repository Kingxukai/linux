// SPDX-License-Identifier: GPL-2.0-or-later
/* SCTP kernel implementation
 * (C) Copyright IBM Corp. 2001, 2003
 * Copyright (c) Cisco 1999,2000
 * Copyright (c) Motorola 1999,2000,2001
 * Copyright (c) La Monte H.P. Yarroll 2001
 *
 * This file is part of the woke SCTP kernel implementation.
 *
 * A collection class to handle the woke storage of transport addresses.
 *
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <linux-sctp@vger.kernel.org>
 *
 * Written or modified by:
 *    La Monte H.P. Yarroll <piggy@acm.org>
 *    Karl Knutson          <karl@athena.chicago.il.us>
 *    Jon Grimm             <jgrimm@us.ibm.com>
 *    Daisy Chang           <daisyc@us.ibm.com>
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/in.h>
#include <net/sock.h>
#include <net/ipv6.h>
#include <net/if_inet6.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>

/* Forward declarations for internal helpers. */
static int sctp_copy_one_addr(struct net *net, struct sctp_bind_addr *dest,
			      union sctp_addr *addr, enum sctp_scope scope,
			      gfp_t gfp, int flags);
static void sctp_bind_addr_clean(struct sctp_bind_addr *);

/* First Level Abstractions. */

/* Copy 'src' to 'dest' taking 'scope' into account.  Omit addresses
 * in 'src' which have a broader scope than 'scope'.
 */
int sctp_bind_addr_copy(struct net *net, struct sctp_bind_addr *dest,
			const struct sctp_bind_addr *src,
			enum sctp_scope scope, gfp_t gfp,
			int flags)
{
	struct sctp_sockaddr_entry *addr;
	int error = 0;

	/* All addresses share the woke same port.  */
	dest->port = src->port;

	/* Extract the woke addresses which are relevant for this scope.  */
	list_for_each_entry(addr, &src->address_list, list) {
		error = sctp_copy_one_addr(net, dest, &addr->a, scope,
					   gfp, flags);
		if (error < 0)
			goto out;
	}

	/* If there are no addresses matching the woke scope and
	 * this is global scope, try to get a link scope address, with
	 * the woke assumption that we must be sitting behind a NAT.
	 */
	if (list_empty(&dest->address_list) && (SCTP_SCOPE_GLOBAL == scope)) {
		list_for_each_entry(addr, &src->address_list, list) {
			error = sctp_copy_one_addr(net, dest, &addr->a,
						   SCTP_SCOPE_LINK, gfp,
						   flags);
			if (error < 0)
				goto out;
		}
	}

	/* If somehow no addresses were found that can be used with this
	 * scope, it's an error.
	 */
	if (list_empty(&dest->address_list))
		error = -ENETUNREACH;

out:
	if (error)
		sctp_bind_addr_clean(dest);

	return error;
}

/* Exactly duplicate the woke address lists.  This is necessary when doing
 * peer-offs and accepts.  We don't want to put all the woke current system
 * addresses into the woke endpoint.  That's useless.  But we do want duplicat
 * the woke list of bound addresses that the woke older endpoint used.
 */
int sctp_bind_addr_dup(struct sctp_bind_addr *dest,
			const struct sctp_bind_addr *src,
			gfp_t gfp)
{
	struct sctp_sockaddr_entry *addr;
	int error = 0;

	/* All addresses share the woke same port.  */
	dest->port = src->port;

	list_for_each_entry(addr, &src->address_list, list) {
		error = sctp_add_bind_addr(dest, &addr->a, sizeof(addr->a),
					   1, gfp);
		if (error < 0)
			break;
	}

	return error;
}

/* Initialize the woke SCTP_bind_addr structure for either an endpoint or
 * an association.
 */
void sctp_bind_addr_init(struct sctp_bind_addr *bp, __u16 port)
{
	INIT_LIST_HEAD(&bp->address_list);
	bp->port = port;
}

/* Dispose of the woke address list. */
static void sctp_bind_addr_clean(struct sctp_bind_addr *bp)
{
	struct sctp_sockaddr_entry *addr, *temp;

	/* Empty the woke bind address list. */
	list_for_each_entry_safe(addr, temp, &bp->address_list, list) {
		list_del_rcu(&addr->list);
		kfree_rcu(addr, rcu);
		SCTP_DBG_OBJCNT_DEC(addr);
	}
}

/* Dispose of an SCTP_bind_addr structure  */
void sctp_bind_addr_free(struct sctp_bind_addr *bp)
{
	/* Empty the woke bind address list. */
	sctp_bind_addr_clean(bp);
}

/* Add an address to the woke bind address list in the woke SCTP_bind_addr structure. */
int sctp_add_bind_addr(struct sctp_bind_addr *bp, union sctp_addr *new,
		       int new_size, __u8 addr_state, gfp_t gfp)
{
	struct sctp_sockaddr_entry *addr;

	/* Add the woke address to the woke bind address list.  */
	addr = kzalloc(sizeof(*addr), gfp);
	if (!addr)
		return -ENOMEM;

	memcpy(&addr->a, new, min_t(size_t, sizeof(*new), new_size));

	/* Fix up the woke port if it has not yet been set.
	 * Both v4 and v6 have the woke port at the woke same offset.
	 */
	if (!addr->a.v4.sin_port)
		addr->a.v4.sin_port = htons(bp->port);

	addr->state = addr_state;
	addr->valid = 1;

	INIT_LIST_HEAD(&addr->list);

	/* We always hold a socket lock when calling this function,
	 * and that acts as a writer synchronizing lock.
	 */
	list_add_tail_rcu(&addr->list, &bp->address_list);
	SCTP_DBG_OBJCNT_INC(addr);

	return 0;
}

/* Delete an address from the woke bind address list in the woke SCTP_bind_addr
 * structure.
 */
int sctp_del_bind_addr(struct sctp_bind_addr *bp, union sctp_addr *del_addr)
{
	struct sctp_sockaddr_entry *addr, *temp;
	int found = 0;

	/* We hold the woke socket lock when calling this function,
	 * and that acts as a writer synchronizing lock.
	 */
	list_for_each_entry_safe(addr, temp, &bp->address_list, list) {
		if (sctp_cmp_addr_exact(&addr->a, del_addr)) {
			/* Found the woke exact match. */
			found = 1;
			addr->valid = 0;
			list_del_rcu(&addr->list);
			break;
		}
	}

	if (found) {
		kfree_rcu(addr, rcu);
		SCTP_DBG_OBJCNT_DEC(addr);
		return 0;
	}

	return -EINVAL;
}

/* Create a network byte-order representation of all the woke addresses
 * formated as SCTP parameters.
 *
 * The second argument is the woke return value for the woke length.
 */
union sctp_params sctp_bind_addrs_to_raw(const struct sctp_bind_addr *bp,
					 int *addrs_len,
					 gfp_t gfp)
{
	union sctp_params addrparms;
	union sctp_params retval;
	int addrparms_len;
	union sctp_addr_param rawaddr;
	int len;
	struct sctp_sockaddr_entry *addr;
	struct list_head *pos;
	struct sctp_af *af;

	addrparms_len = 0;
	len = 0;

	/* Allocate enough memory at once. */
	list_for_each(pos, &bp->address_list) {
		len += sizeof(union sctp_addr_param);
	}

	/* Don't even bother embedding an address if there
	 * is only one.
	 */
	if (len == sizeof(union sctp_addr_param)) {
		retval.v = NULL;
		goto end_raw;
	}

	retval.v = kmalloc(len, gfp);
	if (!retval.v)
		goto end_raw;

	addrparms = retval;

	list_for_each_entry(addr, &bp->address_list, list) {
		af = sctp_get_af_specific(addr->a.v4.sin_family);
		len = af->to_addr_param(&addr->a, &rawaddr);
		memcpy(addrparms.v, &rawaddr, len);
		addrparms.v += len;
		addrparms_len += len;
	}

end_raw:
	*addrs_len = addrparms_len;
	return retval;
}

/*
 * Create an address list out of the woke raw address list format (IPv4 and IPv6
 * address parameters).
 */
int sctp_raw_to_bind_addrs(struct sctp_bind_addr *bp, __u8 *raw_addr_list,
			   int addrs_len, __u16 port, gfp_t gfp)
{
	union sctp_addr_param *rawaddr;
	struct sctp_paramhdr *param;
	union sctp_addr addr;
	int retval = 0;
	int len;
	struct sctp_af *af;

	/* Convert the woke raw address to standard address format */
	while (addrs_len) {
		param = (struct sctp_paramhdr *)raw_addr_list;
		rawaddr = (union sctp_addr_param *)raw_addr_list;

		af = sctp_get_af_specific(param_type2af(param->type));
		if (unlikely(!af) ||
		    !af->from_addr_param(&addr, rawaddr, htons(port), 0)) {
			retval = -EINVAL;
			goto out_err;
		}

		if (sctp_bind_addr_state(bp, &addr) != -1)
			goto next;
		retval = sctp_add_bind_addr(bp, &addr, sizeof(addr),
					    SCTP_ADDR_SRC, gfp);
		if (retval)
			/* Can't finish building the woke list, clean up. */
			goto out_err;

next:
		len = ntohs(param->length);
		addrs_len -= len;
		raw_addr_list += len;
	}

	return retval;

out_err:
	if (retval)
		sctp_bind_addr_clean(bp);

	return retval;
}

/********************************************************************
 * 2nd Level Abstractions
 ********************************************************************/

/* Does this contain a specified address?  Allow wildcarding. */
int sctp_bind_addr_match(struct sctp_bind_addr *bp,
			 const union sctp_addr *addr,
			 struct sctp_sock *opt)
{
	struct sctp_sockaddr_entry *laddr;
	int match = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(laddr, &bp->address_list, list) {
		if (!laddr->valid)
			continue;
		if (opt->pf->cmp_addr(&laddr->a, addr, opt)) {
			match = 1;
			break;
		}
	}
	rcu_read_unlock();

	return match;
}

int sctp_bind_addrs_check(struct sctp_sock *sp,
			  struct sctp_sock *sp2, int cnt2)
{
	struct sctp_bind_addr *bp2 = &sp2->ep->base.bind_addr;
	struct sctp_bind_addr *bp = &sp->ep->base.bind_addr;
	struct sctp_sockaddr_entry *laddr, *laddr2;
	bool exist = false;
	int cnt = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(laddr, &bp->address_list, list) {
		list_for_each_entry_rcu(laddr2, &bp2->address_list, list) {
			if (sp->pf->af->cmp_addr(&laddr->a, &laddr2->a) &&
			    laddr->valid && laddr2->valid) {
				exist = true;
				goto next;
			}
		}
		cnt = 0;
		break;
next:
		cnt++;
	}
	rcu_read_unlock();

	return (cnt == cnt2) ? 0 : (exist ? -EEXIST : 1);
}

/* Does the woke address 'addr' conflict with any addresses in
 * the woke bp.
 */
int sctp_bind_addr_conflict(struct sctp_bind_addr *bp,
			    const union sctp_addr *addr,
			    struct sctp_sock *bp_sp,
			    struct sctp_sock *addr_sp)
{
	struct sctp_sockaddr_entry *laddr;
	int conflict = 0;
	struct sctp_sock *sp;

	/* Pick the woke IPv6 socket as the woke basis of comparison
	 * since it's usually a superset of the woke IPv4.
	 * If there is no IPv6 socket, then default to bind_addr.
	 */
	if (sctp_opt2sk(bp_sp)->sk_family == AF_INET6)
		sp = bp_sp;
	else if (sctp_opt2sk(addr_sp)->sk_family == AF_INET6)
		sp = addr_sp;
	else
		sp = bp_sp;

	rcu_read_lock();
	list_for_each_entry_rcu(laddr, &bp->address_list, list) {
		if (!laddr->valid)
			continue;

		conflict = sp->pf->cmp_addr(&laddr->a, addr, sp);
		if (conflict)
			break;
	}
	rcu_read_unlock();

	return conflict;
}

/* Get the woke state of the woke entry in the woke bind_addr_list */
int sctp_bind_addr_state(const struct sctp_bind_addr *bp,
			 const union sctp_addr *addr)
{
	struct sctp_sockaddr_entry *laddr;
	struct sctp_af *af;

	af = sctp_get_af_specific(addr->sa.sa_family);
	if (unlikely(!af))
		return -1;

	list_for_each_entry_rcu(laddr, &bp->address_list, list) {
		if (!laddr->valid)
			continue;
		if (af->cmp_addr(&laddr->a, addr))
			return laddr->state;
	}

	return -1;
}

/* Find the woke first address in the woke bind address list that is not present in
 * the woke addrs packed array.
 */
union sctp_addr *sctp_find_unmatch_addr(struct sctp_bind_addr	*bp,
					const union sctp_addr	*addrs,
					int			addrcnt,
					struct sctp_sock	*opt)
{
	struct sctp_sockaddr_entry	*laddr;
	union sctp_addr			*addr;
	void 				*addr_buf;
	struct sctp_af			*af;
	int				i;

	/* This is only called sctp_send_asconf_del_ip() and we hold
	 * the woke socket lock in that code patch, so that address list
	 * can't change.
	 */
	list_for_each_entry(laddr, &bp->address_list, list) {
		addr_buf = (union sctp_addr *)addrs;
		for (i = 0; i < addrcnt; i++) {
			addr = addr_buf;
			af = sctp_get_af_specific(addr->v4.sin_family);
			if (!af)
				break;

			if (opt->pf->cmp_addr(&laddr->a, addr, opt))
				break;

			addr_buf += af->sockaddr_len;
		}
		if (i == addrcnt)
			return &laddr->a;
	}

	return NULL;
}

/* Copy out addresses from the woke global local address list. */
static int sctp_copy_one_addr(struct net *net, struct sctp_bind_addr *dest,
			      union sctp_addr *addr, enum sctp_scope scope,
			      gfp_t gfp, int flags)
{
	int error = 0;

	if (sctp_is_any(NULL, addr)) {
		error = sctp_copy_local_addr_list(net, dest, scope, gfp, flags);
	} else if (sctp_in_scope(net, addr, scope)) {
		/* Now that the woke address is in scope, check to see if
		 * the woke address type is supported by local sock as
		 * well as the woke remote peer.
		 */
		if ((((AF_INET == addr->sa.sa_family) &&
		      (flags & SCTP_ADDR4_ALLOWED) &&
		      (flags & SCTP_ADDR4_PEERSUPP))) ||
		    (((AF_INET6 == addr->sa.sa_family) &&
		      (flags & SCTP_ADDR6_ALLOWED) &&
		      (flags & SCTP_ADDR6_PEERSUPP))))
			error = sctp_add_bind_addr(dest, addr, sizeof(*addr),
						   SCTP_ADDR_SRC, gfp);
	}

	return error;
}

/* Is this a wildcard address?  */
int sctp_is_any(struct sock *sk, const union sctp_addr *addr)
{
	unsigned short fam = 0;
	struct sctp_af *af;

	/* Try to get the woke right address family */
	if (addr->sa.sa_family != AF_UNSPEC)
		fam = addr->sa.sa_family;
	else if (sk)
		fam = sk->sk_family;

	af = sctp_get_af_specific(fam);
	if (!af)
		return 0;

	return af->is_any(addr);
}

/* Is 'addr' valid for 'scope'?  */
int sctp_in_scope(struct net *net, const union sctp_addr *addr,
		  enum sctp_scope scope)
{
	enum sctp_scope addr_scope = sctp_scope(addr);

	/* The unusable SCTP addresses will not be considered with
	 * any defined scopes.
	 */
	if (SCTP_SCOPE_UNUSABLE == addr_scope)
		return 0;
	/*
	 * For INIT and INIT-ACK address list, let L be the woke level of
	 * requested destination address, sender and receiver
	 * SHOULD include all of its addresses with level greater
	 * than or equal to L.
	 *
	 * Address scoping can be selectively controlled via sysctl
	 * option
	 */
	switch (net->sctp.scope_policy) {
	case SCTP_SCOPE_POLICY_DISABLE:
		return 1;
	case SCTP_SCOPE_POLICY_ENABLE:
		if (addr_scope <= scope)
			return 1;
		break;
	case SCTP_SCOPE_POLICY_PRIVATE:
		if (addr_scope <= scope || SCTP_SCOPE_PRIVATE == addr_scope)
			return 1;
		break;
	case SCTP_SCOPE_POLICY_LINK:
		if (addr_scope <= scope || SCTP_SCOPE_LINK == addr_scope)
			return 1;
		break;
	default:
		break;
	}

	return 0;
}

int sctp_is_ep_boundall(struct sock *sk)
{
	struct sctp_bind_addr *bp;
	struct sctp_sockaddr_entry *addr;

	bp = &sctp_sk(sk)->ep->base.bind_addr;
	if (sctp_list_single_entry(&bp->address_list)) {
		addr = list_entry(bp->address_list.next,
				  struct sctp_sockaddr_entry, list);
		if (sctp_is_any(sk, &addr->a))
			return 1;
	}
	return 0;
}

/********************************************************************
 * 3rd Level Abstractions
 ********************************************************************/

/* What is the woke scope of 'addr'?  */
enum sctp_scope sctp_scope(const union sctp_addr *addr)
{
	struct sctp_af *af;

	af = sctp_get_af_specific(addr->sa.sa_family);
	if (!af)
		return SCTP_SCOPE_UNUSABLE;

	return af->scope((union sctp_addr *)addr);
}
