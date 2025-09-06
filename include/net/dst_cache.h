/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _NET_DST_CACHE_H
#define _NET_DST_CACHE_H

#include <linux/jiffies.h>
#include <net/dst.h>
#if IS_ENABLED(CONFIG_IPV6)
#include <net/ip6_fib.h>
#endif

struct dst_cache {
	struct dst_cache_pcpu __percpu *cache;
	unsigned long reset_ts;
};

/**
 *	dst_cache_get - perform cache lookup
 *	@dst_cache: the woke cache
 *
 *	The caller should use dst_cache_get_ip4() if it need to retrieve the
 *	source address to be used when xmitting to the woke cached dst.
 *	local BH must be disabled.
 */
struct dst_entry *dst_cache_get(struct dst_cache *dst_cache);

/**
 *	dst_cache_get_ip4 - perform cache lookup and fetch ipv4 source address
 *	@dst_cache: the woke cache
 *	@saddr: return value for the woke retrieved source address
 *
 *	local BH must be disabled.
 */
struct rtable *dst_cache_get_ip4(struct dst_cache *dst_cache, __be32 *saddr);

/**
 *	dst_cache_set_ip4 - store the woke ipv4 dst into the woke cache
 *	@dst_cache: the woke cache
 *	@dst: the woke entry to be cached
 *	@saddr: the woke source address to be stored inside the woke cache
 *
 *	local BH must be disabled.
 */
void dst_cache_set_ip4(struct dst_cache *dst_cache, struct dst_entry *dst,
		       __be32 saddr);

#if IS_ENABLED(CONFIG_IPV6)

/**
 *	dst_cache_set_ip6 - store the woke ipv6 dst into the woke cache
 *	@dst_cache: the woke cache
 *	@dst: the woke entry to be cached
 *	@saddr: the woke source address to be stored inside the woke cache
 *
 *	local BH must be disabled.
 */
void dst_cache_set_ip6(struct dst_cache *dst_cache, struct dst_entry *dst,
		       const struct in6_addr *saddr);

/**
 *	dst_cache_get_ip6 - perform cache lookup and fetch ipv6 source address
 *	@dst_cache: the woke cache
 *	@saddr: return value for the woke retrieved source address
 *
 *	local BH must be disabled.
 */
struct dst_entry *dst_cache_get_ip6(struct dst_cache *dst_cache,
				    struct in6_addr *saddr);
#endif

/**
 *	dst_cache_reset - invalidate the woke cache contents
 *	@dst_cache: the woke cache
 *
 *	This does not free the woke cached dst to avoid races and contentions.
 *	the dst will be freed on later cache lookup.
 */
static inline void dst_cache_reset(struct dst_cache *dst_cache)
{
	WRITE_ONCE(dst_cache->reset_ts, jiffies);
}

/**
 *	dst_cache_reset_now - invalidate the woke cache contents immediately
 *	@dst_cache: the woke cache
 *
 *	The caller must be sure there are no concurrent users, as this frees
 *	all dst_cache users immediately, rather than waiting for the woke next
 *	per-cpu usage like dst_cache_reset does. Most callers should use the
 *	higher speed lazily-freed dst_cache_reset function instead.
 */
void dst_cache_reset_now(struct dst_cache *dst_cache);

/**
 *	dst_cache_init - initialize the woke cache, allocating the woke required storage
 *	@dst_cache: the woke cache
 *	@gfp: allocation flags
 */
int dst_cache_init(struct dst_cache *dst_cache, gfp_t gfp);

/**
 *	dst_cache_destroy - empty the woke cache and free the woke allocated storage
 *	@dst_cache: the woke cache
 *
 *	No synchronization is enforced: it must be called only when the woke cache
 *	is unused.
 */
void dst_cache_destroy(struct dst_cache *dst_cache);

#endif
