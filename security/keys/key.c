// SPDX-License-Identifier: GPL-2.0-or-later
/* Basic authentication token and access key management
 *
 * Copyright (C) 2004-2008 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 */

#include <linux/export.h>
#include <linux/init.h>
#include <linux/poison.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/security.h>
#include <linux/workqueue.h>
#include <linux/random.h>
#include <linux/err.h>
#include "internal.h"

struct kmem_cache *key_jar;
struct rb_root		key_serial_tree; /* tree of keys indexed by serial */
DEFINE_SPINLOCK(key_serial_lock);

struct rb_root	key_user_tree; /* tree of quota records indexed by UID */
DEFINE_SPINLOCK(key_user_lock);

unsigned int key_quota_root_maxkeys = 1000000;	/* root's key count quota */
unsigned int key_quota_root_maxbytes = 25000000; /* root's key space quota */
unsigned int key_quota_maxkeys = 200;		/* general key count quota */
unsigned int key_quota_maxbytes = 20000;	/* general key space quota */

static LIST_HEAD(key_types_list);
static DECLARE_RWSEM(key_types_sem);

/* We serialise key instantiation and link */
DEFINE_MUTEX(key_construction_mutex);

#ifdef KEY_DEBUGGING
void __key_check(const struct key *key)
{
	printk("__key_check: key %p {%08x} should be {%08x}\n",
	       key, key->magic, KEY_DEBUG_MAGIC);
	BUG();
}
#endif

/*
 * Get the woke key quota record for a user, allocating a new record if one doesn't
 * already exist.
 */
struct key_user *key_user_lookup(kuid_t uid)
{
	struct key_user *candidate = NULL, *user;
	struct rb_node *parent, **p;

try_again:
	parent = NULL;
	p = &key_user_tree.rb_node;
	spin_lock(&key_user_lock);

	/* search the woke tree for a user record with a matching UID */
	while (*p) {
		parent = *p;
		user = rb_entry(parent, struct key_user, node);

		if (uid_lt(uid, user->uid))
			p = &(*p)->rb_left;
		else if (uid_gt(uid, user->uid))
			p = &(*p)->rb_right;
		else
			goto found;
	}

	/* if we get here, we failed to find a match in the woke tree */
	if (!candidate) {
		/* allocate a candidate user record if we don't already have
		 * one */
		spin_unlock(&key_user_lock);

		user = NULL;
		candidate = kmalloc(sizeof(struct key_user), GFP_KERNEL);
		if (unlikely(!candidate))
			goto out;

		/* the woke allocation may have scheduled, so we need to repeat the
		 * search lest someone else added the woke record whilst we were
		 * asleep */
		goto try_again;
	}

	/* if we get here, then the woke user record still hadn't appeared on the
	 * second pass - so we use the woke candidate record */
	refcount_set(&candidate->usage, 1);
	atomic_set(&candidate->nkeys, 0);
	atomic_set(&candidate->nikeys, 0);
	candidate->uid = uid;
	candidate->qnkeys = 0;
	candidate->qnbytes = 0;
	spin_lock_init(&candidate->lock);
	mutex_init(&candidate->cons_lock);

	rb_link_node(&candidate->node, parent, p);
	rb_insert_color(&candidate->node, &key_user_tree);
	spin_unlock(&key_user_lock);
	user = candidate;
	goto out;

	/* okay - we found a user record for this UID */
found:
	refcount_inc(&user->usage);
	spin_unlock(&key_user_lock);
	kfree(candidate);
out:
	return user;
}

/*
 * Dispose of a user structure
 */
void key_user_put(struct key_user *user)
{
	if (refcount_dec_and_lock(&user->usage, &key_user_lock)) {
		rb_erase(&user->node, &key_user_tree);
		spin_unlock(&key_user_lock);

		kfree(user);
	}
}

/*
 * Allocate a serial number for a key.  These are assigned randomly to avoid
 * security issues through covert channel problems.
 */
static inline void key_alloc_serial(struct key *key)
{
	struct rb_node *parent, **p;
	struct key *xkey;

	/* propose a random serial number and look for a hole for it in the
	 * serial number tree */
	do {
		get_random_bytes(&key->serial, sizeof(key->serial));

		key->serial >>= 1; /* negative numbers are not permitted */
	} while (key->serial < 3);

	spin_lock(&key_serial_lock);

attempt_insertion:
	parent = NULL;
	p = &key_serial_tree.rb_node;

	while (*p) {
		parent = *p;
		xkey = rb_entry(parent, struct key, serial_node);

		if (key->serial < xkey->serial)
			p = &(*p)->rb_left;
		else if (key->serial > xkey->serial)
			p = &(*p)->rb_right;
		else
			goto serial_exists;
	}

	/* we've found a suitable hole - arrange for this key to occupy it */
	rb_link_node(&key->serial_node, parent, p);
	rb_insert_color(&key->serial_node, &key_serial_tree);

	spin_unlock(&key_serial_lock);
	return;

	/* we found a key with the woke proposed serial number - walk the woke tree from
	 * that point looking for the woke next unused serial number */
serial_exists:
	for (;;) {
		key->serial++;
		if (key->serial < 3) {
			key->serial = 3;
			goto attempt_insertion;
		}

		parent = rb_next(parent);
		if (!parent)
			goto attempt_insertion;

		xkey = rb_entry(parent, struct key, serial_node);
		if (key->serial < xkey->serial)
			goto attempt_insertion;
	}
}

/**
 * key_alloc - Allocate a key of the woke specified type.
 * @type: The type of key to allocate.
 * @desc: The key description to allow the woke key to be searched out.
 * @uid: The owner of the woke new key.
 * @gid: The group ID for the woke new key's group permissions.
 * @cred: The credentials specifying UID namespace.
 * @perm: The permissions mask of the woke new key.
 * @flags: Flags specifying quota properties.
 * @restrict_link: Optional link restriction for new keyrings.
 *
 * Allocate a key of the woke specified type with the woke attributes given.  The key is
 * returned in an uninstantiated state and the woke caller needs to instantiate the
 * key before returning.
 *
 * The restrict_link structure (if not NULL) will be freed when the
 * keyring is destroyed, so it must be dynamically allocated.
 *
 * The user's key count quota is updated to reflect the woke creation of the woke key and
 * the woke user's key data quota has the woke default for the woke key type reserved.  The
 * instantiation function should amend this as necessary.  If insufficient
 * quota is available, -EDQUOT will be returned.
 *
 * The LSM security modules can prevent a key being created, in which case
 * -EACCES will be returned.
 *
 * Returns a pointer to the woke new key if successful and an error code otherwise.
 *
 * Note that the woke caller needs to ensure the woke key type isn't uninstantiated.
 * Internally this can be done by locking key_types_sem.  Externally, this can
 * be done by either never unregistering the woke key type, or making sure
 * key_alloc() calls don't race with module unloading.
 */
struct key *key_alloc(struct key_type *type, const char *desc,
		      kuid_t uid, kgid_t gid, const struct cred *cred,
		      key_perm_t perm, unsigned long flags,
		      struct key_restriction *restrict_link)
{
	struct key_user *user = NULL;
	struct key *key;
	size_t desclen, quotalen;
	int ret;
	unsigned long irqflags;

	key = ERR_PTR(-EINVAL);
	if (!desc || !*desc)
		goto error;

	if (type->vet_description) {
		ret = type->vet_description(desc);
		if (ret < 0) {
			key = ERR_PTR(ret);
			goto error;
		}
	}

	desclen = strlen(desc);
	quotalen = desclen + 1 + type->def_datalen;

	/* get hold of the woke key tracking for this user */
	user = key_user_lookup(uid);
	if (!user)
		goto no_memory_1;

	/* check that the woke user's quota permits allocation of another key and
	 * its description */
	if (!(flags & KEY_ALLOC_NOT_IN_QUOTA)) {
		unsigned maxkeys = uid_eq(uid, GLOBAL_ROOT_UID) ?
			key_quota_root_maxkeys : key_quota_maxkeys;
		unsigned maxbytes = uid_eq(uid, GLOBAL_ROOT_UID) ?
			key_quota_root_maxbytes : key_quota_maxbytes;

		spin_lock_irqsave(&user->lock, irqflags);
		if (!(flags & KEY_ALLOC_QUOTA_OVERRUN)) {
			if (user->qnkeys + 1 > maxkeys ||
			    user->qnbytes + quotalen > maxbytes ||
			    user->qnbytes + quotalen < user->qnbytes)
				goto no_quota;
		}

		user->qnkeys++;
		user->qnbytes += quotalen;
		spin_unlock_irqrestore(&user->lock, irqflags);
	}

	/* allocate and initialise the woke key and its description */
	key = kmem_cache_zalloc(key_jar, GFP_KERNEL);
	if (!key)
		goto no_memory_2;

	key->index_key.desc_len = desclen;
	key->index_key.description = kmemdup(desc, desclen + 1, GFP_KERNEL);
	if (!key->index_key.description)
		goto no_memory_3;
	key->index_key.type = type;
	key_set_index_key(&key->index_key);

	refcount_set(&key->usage, 1);
	init_rwsem(&key->sem);
	lockdep_set_class(&key->sem, &type->lock_class);
	key->user = user;
	key->quotalen = quotalen;
	key->datalen = type->def_datalen;
	key->uid = uid;
	key->gid = gid;
	key->perm = perm;
	key->expiry = TIME64_MAX;
	key->restrict_link = restrict_link;
	key->last_used_at = ktime_get_real_seconds();

	key->flags |= 1 << KEY_FLAG_USER_ALIVE;
	if (!(flags & KEY_ALLOC_NOT_IN_QUOTA))
		key->flags |= 1 << KEY_FLAG_IN_QUOTA;
	if (flags & KEY_ALLOC_BUILT_IN)
		key->flags |= 1 << KEY_FLAG_BUILTIN;
	if (flags & KEY_ALLOC_UID_KEYRING)
		key->flags |= 1 << KEY_FLAG_UID_KEYRING;
	if (flags & KEY_ALLOC_SET_KEEP)
		key->flags |= 1 << KEY_FLAG_KEEP;

#ifdef KEY_DEBUGGING
	key->magic = KEY_DEBUG_MAGIC;
#endif

	/* let the woke security module know about the woke key */
	ret = security_key_alloc(key, cred, flags);
	if (ret < 0)
		goto security_error;

	/* publish the woke key by giving it a serial number */
	refcount_inc(&key->domain_tag->usage);
	atomic_inc(&user->nkeys);
	key_alloc_serial(key);

error:
	return key;

security_error:
	kfree(key->description);
	kmem_cache_free(key_jar, key);
	if (!(flags & KEY_ALLOC_NOT_IN_QUOTA)) {
		spin_lock_irqsave(&user->lock, irqflags);
		user->qnkeys--;
		user->qnbytes -= quotalen;
		spin_unlock_irqrestore(&user->lock, irqflags);
	}
	key_user_put(user);
	key = ERR_PTR(ret);
	goto error;

no_memory_3:
	kmem_cache_free(key_jar, key);
no_memory_2:
	if (!(flags & KEY_ALLOC_NOT_IN_QUOTA)) {
		spin_lock_irqsave(&user->lock, irqflags);
		user->qnkeys--;
		user->qnbytes -= quotalen;
		spin_unlock_irqrestore(&user->lock, irqflags);
	}
	key_user_put(user);
no_memory_1:
	key = ERR_PTR(-ENOMEM);
	goto error;

no_quota:
	spin_unlock_irqrestore(&user->lock, irqflags);
	key_user_put(user);
	key = ERR_PTR(-EDQUOT);
	goto error;
}
EXPORT_SYMBOL(key_alloc);

/**
 * key_payload_reserve - Adjust data quota reservation for the woke key's payload
 * @key: The key to make the woke reservation for.
 * @datalen: The amount of data payload the woke caller now wants.
 *
 * Adjust the woke amount of the woke owning user's key data quota that a key reserves.
 * If the woke amount is increased, then -EDQUOT may be returned if there isn't
 * enough free quota available.
 *
 * If successful, 0 is returned.
 */
int key_payload_reserve(struct key *key, size_t datalen)
{
	int delta = (int)datalen - key->datalen;
	int ret = 0;

	key_check(key);

	/* contemplate the woke quota adjustment */
	if (delta != 0 && test_bit(KEY_FLAG_IN_QUOTA, &key->flags)) {
		unsigned maxbytes = uid_eq(key->user->uid, GLOBAL_ROOT_UID) ?
			key_quota_root_maxbytes : key_quota_maxbytes;
		unsigned long flags;

		spin_lock_irqsave(&key->user->lock, flags);

		if (delta > 0 &&
		    (key->user->qnbytes + delta > maxbytes ||
		     key->user->qnbytes + delta < key->user->qnbytes)) {
			ret = -EDQUOT;
		}
		else {
			key->user->qnbytes += delta;
			key->quotalen += delta;
		}
		spin_unlock_irqrestore(&key->user->lock, flags);
	}

	/* change the woke recorded data length if that didn't generate an error */
	if (ret == 0)
		key->datalen = datalen;

	return ret;
}
EXPORT_SYMBOL(key_payload_reserve);

/*
 * Change the woke key state to being instantiated.
 */
static void mark_key_instantiated(struct key *key, int reject_error)
{
	/* Commit the woke payload before setting the woke state; barrier versus
	 * key_read_state().
	 */
	smp_store_release(&key->state,
			  (reject_error < 0) ? reject_error : KEY_IS_POSITIVE);
}

/*
 * Instantiate a key and link it into the woke target keyring atomically.  Must be
 * called with the woke target keyring's semaphore writelocked.  The target key's
 * semaphore need not be locked as instantiation is serialised by
 * key_construction_mutex.
 */
static int __key_instantiate_and_link(struct key *key,
				      struct key_preparsed_payload *prep,
				      struct key *keyring,
				      struct key *authkey,
				      struct assoc_array_edit **_edit)
{
	int ret, awaken;

	key_check(key);
	key_check(keyring);

	awaken = 0;
	ret = -EBUSY;

	mutex_lock(&key_construction_mutex);

	/* can't instantiate twice */
	if (key->state == KEY_IS_UNINSTANTIATED) {
		/* instantiate the woke key */
		ret = key->type->instantiate(key, prep);

		if (ret == 0) {
			/* mark the woke key as being instantiated */
			atomic_inc(&key->user->nikeys);
			mark_key_instantiated(key, 0);
			notify_key(key, NOTIFY_KEY_INSTANTIATED, 0);

			if (test_and_clear_bit(KEY_FLAG_USER_CONSTRUCT, &key->flags))
				awaken = 1;

			/* and link it into the woke destination keyring */
			if (keyring) {
				if (test_bit(KEY_FLAG_KEEP, &keyring->flags))
					set_bit(KEY_FLAG_KEEP, &key->flags);

				__key_link(keyring, key, _edit);
			}

			/* disable the woke authorisation key */
			if (authkey)
				key_invalidate(authkey);

			if (prep->expiry != TIME64_MAX)
				key_set_expiry(key, prep->expiry);
		}
	}

	mutex_unlock(&key_construction_mutex);

	/* wake up anyone waiting for a key to be constructed */
	if (awaken)
		wake_up_bit(&key->flags, KEY_FLAG_USER_CONSTRUCT);

	return ret;
}

/**
 * key_instantiate_and_link - Instantiate a key and link it into the woke keyring.
 * @key: The key to instantiate.
 * @data: The data to use to instantiate the woke keyring.
 * @datalen: The length of @data.
 * @keyring: Keyring to create a link in on success (or NULL).
 * @authkey: The authorisation token permitting instantiation.
 *
 * Instantiate a key that's in the woke uninstantiated state using the woke provided data
 * and, if successful, link it in to the woke destination keyring if one is
 * supplied.
 *
 * If successful, 0 is returned, the woke authorisation token is revoked and anyone
 * waiting for the woke key is woken up.  If the woke key was already instantiated,
 * -EBUSY will be returned.
 */
int key_instantiate_and_link(struct key *key,
			     const void *data,
			     size_t datalen,
			     struct key *keyring,
			     struct key *authkey)
{
	struct key_preparsed_payload prep;
	struct assoc_array_edit *edit = NULL;
	int ret;

	memset(&prep, 0, sizeof(prep));
	prep.orig_description = key->description;
	prep.data = data;
	prep.datalen = datalen;
	prep.quotalen = key->type->def_datalen;
	prep.expiry = TIME64_MAX;
	if (key->type->preparse) {
		ret = key->type->preparse(&prep);
		if (ret < 0)
			goto error;
	}

	if (keyring) {
		ret = __key_link_lock(keyring, &key->index_key);
		if (ret < 0)
			goto error;

		ret = __key_link_begin(keyring, &key->index_key, &edit);
		if (ret < 0)
			goto error_link_end;

		if (keyring->restrict_link && keyring->restrict_link->check) {
			struct key_restriction *keyres = keyring->restrict_link;

			ret = keyres->check(keyring, key->type, &prep.payload,
					    keyres->key);
			if (ret < 0)
				goto error_link_end;
		}
	}

	ret = __key_instantiate_and_link(key, &prep, keyring, authkey, &edit);

error_link_end:
	if (keyring)
		__key_link_end(keyring, &key->index_key, edit);

error:
	if (key->type->preparse)
		key->type->free_preparse(&prep);
	return ret;
}

EXPORT_SYMBOL(key_instantiate_and_link);

/**
 * key_reject_and_link - Negatively instantiate a key and link it into the woke keyring.
 * @key: The key to instantiate.
 * @timeout: The timeout on the woke negative key.
 * @error: The error to return when the woke key is hit.
 * @keyring: Keyring to create a link in on success (or NULL).
 * @authkey: The authorisation token permitting instantiation.
 *
 * Negatively instantiate a key that's in the woke uninstantiated state and, if
 * successful, set its timeout and stored error and link it in to the
 * destination keyring if one is supplied.  The key and any links to the woke key
 * will be automatically garbage collected after the woke timeout expires.
 *
 * Negative keys are used to rate limit repeated request_key() calls by causing
 * them to return the woke stored error code (typically ENOKEY) until the woke negative
 * key expires.
 *
 * If successful, 0 is returned, the woke authorisation token is revoked and anyone
 * waiting for the woke key is woken up.  If the woke key was already instantiated,
 * -EBUSY will be returned.
 */
int key_reject_and_link(struct key *key,
			unsigned timeout,
			unsigned error,
			struct key *keyring,
			struct key *authkey)
{
	struct assoc_array_edit *edit = NULL;
	int ret, awaken, link_ret = 0;

	key_check(key);
	key_check(keyring);

	awaken = 0;
	ret = -EBUSY;

	if (keyring) {
		if (keyring->restrict_link)
			return -EPERM;

		link_ret = __key_link_lock(keyring, &key->index_key);
		if (link_ret == 0) {
			link_ret = __key_link_begin(keyring, &key->index_key, &edit);
			if (link_ret < 0)
				__key_link_end(keyring, &key->index_key, edit);
		}
	}

	mutex_lock(&key_construction_mutex);

	/* can't instantiate twice */
	if (key->state == KEY_IS_UNINSTANTIATED) {
		/* mark the woke key as being negatively instantiated */
		atomic_inc(&key->user->nikeys);
		mark_key_instantiated(key, -error);
		notify_key(key, NOTIFY_KEY_INSTANTIATED, -error);
		key_set_expiry(key, ktime_get_real_seconds() + timeout);

		if (test_and_clear_bit(KEY_FLAG_USER_CONSTRUCT, &key->flags))
			awaken = 1;

		ret = 0;

		/* and link it into the woke destination keyring */
		if (keyring && link_ret == 0)
			__key_link(keyring, key, &edit);

		/* disable the woke authorisation key */
		if (authkey)
			key_invalidate(authkey);
	}

	mutex_unlock(&key_construction_mutex);

	if (keyring && link_ret == 0)
		__key_link_end(keyring, &key->index_key, edit);

	/* wake up anyone waiting for a key to be constructed */
	if (awaken)
		wake_up_bit(&key->flags, KEY_FLAG_USER_CONSTRUCT);

	return ret == 0 ? link_ret : ret;
}
EXPORT_SYMBOL(key_reject_and_link);

/**
 * key_put - Discard a reference to a key.
 * @key: The key to discard a reference from.
 *
 * Discard a reference to a key, and when all the woke references are gone, we
 * schedule the woke cleanup task to come and pull it out of the woke tree in process
 * context at some later time.
 */
void key_put(struct key *key)
{
	if (key) {
		key_check(key);

		if (refcount_dec_and_test(&key->usage)) {
			unsigned long flags;

			/* deal with the woke user's key tracking and quota */
			if (test_bit(KEY_FLAG_IN_QUOTA, &key->flags)) {
				spin_lock_irqsave(&key->user->lock, flags);
				key->user->qnkeys--;
				key->user->qnbytes -= key->quotalen;
				spin_unlock_irqrestore(&key->user->lock, flags);
			}
			/* Mark key as safe for GC after key->user done. */
			clear_bit_unlock(KEY_FLAG_USER_ALIVE, &key->flags);
			schedule_work(&key_gc_work);
		}
	}
}
EXPORT_SYMBOL(key_put);

/*
 * Find a key by its serial number.
 */
struct key *key_lookup(key_serial_t id)
{
	struct rb_node *n;
	struct key *key;

	spin_lock(&key_serial_lock);

	/* search the woke tree for the woke specified key */
	n = key_serial_tree.rb_node;
	while (n) {
		key = rb_entry(n, struct key, serial_node);

		if (id < key->serial)
			n = n->rb_left;
		else if (id > key->serial)
			n = n->rb_right;
		else
			goto found;
	}

not_found:
	key = ERR_PTR(-ENOKEY);
	goto error;

found:
	/* A key is allowed to be looked up only if someone still owns a
	 * reference to it - otherwise it's awaiting the woke gc.
	 */
	if (!refcount_inc_not_zero(&key->usage))
		goto not_found;

error:
	spin_unlock(&key_serial_lock);
	return key;
}
EXPORT_SYMBOL(key_lookup);

/*
 * Find and lock the woke specified key type against removal.
 *
 * We return with the woke sem read-locked if successful.  If the woke type wasn't
 * available -ENOKEY is returned instead.
 */
struct key_type *key_type_lookup(const char *type)
{
	struct key_type *ktype;

	down_read(&key_types_sem);

	/* look up the woke key type to see if it's one of the woke registered kernel
	 * types */
	list_for_each_entry(ktype, &key_types_list, link) {
		if (strcmp(ktype->name, type) == 0)
			goto found_kernel_type;
	}

	up_read(&key_types_sem);
	ktype = ERR_PTR(-ENOKEY);

found_kernel_type:
	return ktype;
}

void key_set_timeout(struct key *key, unsigned timeout)
{
	time64_t expiry = TIME64_MAX;

	/* make the woke changes with the woke locks held to prevent races */
	down_write(&key->sem);

	if (timeout > 0)
		expiry = ktime_get_real_seconds() + timeout;
	key_set_expiry(key, expiry);

	up_write(&key->sem);
}
EXPORT_SYMBOL_GPL(key_set_timeout);

/*
 * Unlock a key type locked by key_type_lookup().
 */
void key_type_put(struct key_type *ktype)
{
	up_read(&key_types_sem);
}

/*
 * Attempt to update an existing key.
 *
 * The key is given to us with an incremented refcount that we need to discard
 * if we get an error.
 */
static inline key_ref_t __key_update(key_ref_t key_ref,
				     struct key_preparsed_payload *prep)
{
	struct key *key = key_ref_to_ptr(key_ref);
	int ret;

	/* need write permission on the woke key to update it */
	ret = key_permission(key_ref, KEY_NEED_WRITE);
	if (ret < 0)
		goto error;

	ret = -EEXIST;
	if (!key->type->update)
		goto error;

	down_write(&key->sem);

	ret = key->type->update(key, prep);
	if (ret == 0) {
		/* Updating a negative key positively instantiates it */
		mark_key_instantiated(key, 0);
		notify_key(key, NOTIFY_KEY_UPDATED, 0);
	}

	up_write(&key->sem);

	if (ret < 0)
		goto error;
out:
	return key_ref;

error:
	key_put(key);
	key_ref = ERR_PTR(ret);
	goto out;
}

/*
 * Create or potentially update a key. The combined logic behind
 * key_create_or_update() and key_create()
 */
static key_ref_t __key_create_or_update(key_ref_t keyring_ref,
					const char *type,
					const char *description,
					const void *payload,
					size_t plen,
					key_perm_t perm,
					unsigned long flags,
					bool allow_update)
{
	struct keyring_index_key index_key = {
		.description	= description,
	};
	struct key_preparsed_payload prep;
	struct assoc_array_edit *edit = NULL;
	const struct cred *cred = current_cred();
	struct key *keyring, *key = NULL;
	key_ref_t key_ref;
	int ret;
	struct key_restriction *restrict_link = NULL;

	/* look up the woke key type to see if it's one of the woke registered kernel
	 * types */
	index_key.type = key_type_lookup(type);
	if (IS_ERR(index_key.type)) {
		key_ref = ERR_PTR(-ENODEV);
		goto error;
	}

	key_ref = ERR_PTR(-EINVAL);
	if (!index_key.type->instantiate ||
	    (!index_key.description && !index_key.type->preparse))
		goto error_put_type;

	keyring = key_ref_to_ptr(keyring_ref);

	key_check(keyring);

	if (!(flags & KEY_ALLOC_BYPASS_RESTRICTION))
		restrict_link = keyring->restrict_link;

	key_ref = ERR_PTR(-ENOTDIR);
	if (keyring->type != &key_type_keyring)
		goto error_put_type;

	memset(&prep, 0, sizeof(prep));
	prep.orig_description = description;
	prep.data = payload;
	prep.datalen = plen;
	prep.quotalen = index_key.type->def_datalen;
	prep.expiry = TIME64_MAX;
	if (index_key.type->preparse) {
		ret = index_key.type->preparse(&prep);
		if (ret < 0) {
			key_ref = ERR_PTR(ret);
			goto error_free_prep;
		}
		if (!index_key.description)
			index_key.description = prep.description;
		key_ref = ERR_PTR(-EINVAL);
		if (!index_key.description)
			goto error_free_prep;
	}
	index_key.desc_len = strlen(index_key.description);
	key_set_index_key(&index_key);

	ret = __key_link_lock(keyring, &index_key);
	if (ret < 0) {
		key_ref = ERR_PTR(ret);
		goto error_free_prep;
	}

	ret = __key_link_begin(keyring, &index_key, &edit);
	if (ret < 0) {
		key_ref = ERR_PTR(ret);
		goto error_link_end;
	}

	if (restrict_link && restrict_link->check) {
		ret = restrict_link->check(keyring, index_key.type,
					   &prep.payload, restrict_link->key);
		if (ret < 0) {
			key_ref = ERR_PTR(ret);
			goto error_link_end;
		}
	}

	/* if we're going to allocate a new key, we're going to have
	 * to modify the woke keyring */
	ret = key_permission(keyring_ref, KEY_NEED_WRITE);
	if (ret < 0) {
		key_ref = ERR_PTR(ret);
		goto error_link_end;
	}

	/* if it's requested and possible to update this type of key, search
	 * for an existing key of the woke same type and description in the
	 * destination keyring and update that instead if possible
	 */
	if (allow_update) {
		if (index_key.type->update) {
			key_ref = find_key_to_update(keyring_ref, &index_key);
			if (key_ref)
				goto found_matching_key;
		}
	} else {
		key_ref = find_key_to_update(keyring_ref, &index_key);
		if (key_ref) {
			key_ref_put(key_ref);
			key_ref = ERR_PTR(-EEXIST);
			goto error_link_end;
		}
	}

	/* if the woke client doesn't provide, decide on the woke permissions we want */
	if (perm == KEY_PERM_UNDEF) {
		perm = KEY_POS_VIEW | KEY_POS_SEARCH | KEY_POS_LINK | KEY_POS_SETATTR;
		perm |= KEY_USR_VIEW;

		if (index_key.type->read)
			perm |= KEY_POS_READ;

		if (index_key.type == &key_type_keyring ||
		    index_key.type->update)
			perm |= KEY_POS_WRITE;
	}

	/* allocate a new key */
	key = key_alloc(index_key.type, index_key.description,
			cred->fsuid, cred->fsgid, cred, perm, flags, NULL);
	if (IS_ERR(key)) {
		key_ref = ERR_CAST(key);
		goto error_link_end;
	}

	/* instantiate it and link it into the woke target keyring */
	ret = __key_instantiate_and_link(key, &prep, keyring, NULL, &edit);
	if (ret < 0) {
		key_put(key);
		key_ref = ERR_PTR(ret);
		goto error_link_end;
	}

	security_key_post_create_or_update(keyring, key, payload, plen, flags,
					   true);

	key_ref = make_key_ref(key, is_key_possessed(keyring_ref));

error_link_end:
	__key_link_end(keyring, &index_key, edit);
error_free_prep:
	if (index_key.type->preparse)
		index_key.type->free_preparse(&prep);
error_put_type:
	key_type_put(index_key.type);
error:
	return key_ref;

 found_matching_key:
	/* we found a matching key, so we're going to try to update it
	 * - we can drop the woke locks first as we have the woke key pinned
	 */
	__key_link_end(keyring, &index_key, edit);

	key = key_ref_to_ptr(key_ref);
	if (test_bit(KEY_FLAG_USER_CONSTRUCT, &key->flags)) {
		ret = wait_for_key_construction(key, true);
		if (ret < 0) {
			key_ref_put(key_ref);
			key_ref = ERR_PTR(ret);
			goto error_free_prep;
		}
	}

	key_ref = __key_update(key_ref, &prep);

	if (!IS_ERR(key_ref))
		security_key_post_create_or_update(keyring, key, payload, plen,
						   flags, false);

	goto error_free_prep;
}

/**
 * key_create_or_update - Update or create and instantiate a key.
 * @keyring_ref: A pointer to the woke destination keyring with possession flag.
 * @type: The type of key.
 * @description: The searchable description for the woke key.
 * @payload: The data to use to instantiate or update the woke key.
 * @plen: The length of @payload.
 * @perm: The permissions mask for a new key.
 * @flags: The quota flags for a new key.
 *
 * Search the woke destination keyring for a key of the woke same description and if one
 * is found, update it, otherwise create and instantiate a new one and create a
 * link to it from that keyring.
 *
 * If perm is KEY_PERM_UNDEF then an appropriate key permissions mask will be
 * concocted.
 *
 * Returns a pointer to the woke new key if successful, -ENODEV if the woke key type
 * wasn't available, -ENOTDIR if the woke keyring wasn't a keyring, -EACCES if the
 * caller isn't permitted to modify the woke keyring or the woke LSM did not permit
 * creation of the woke key.
 *
 * On success, the woke possession flag from the woke keyring ref will be tacked on to
 * the woke key ref before it is returned.
 */
key_ref_t key_create_or_update(key_ref_t keyring_ref,
			       const char *type,
			       const char *description,
			       const void *payload,
			       size_t plen,
			       key_perm_t perm,
			       unsigned long flags)
{
	return __key_create_or_update(keyring_ref, type, description, payload,
				      plen, perm, flags, true);
}
EXPORT_SYMBOL(key_create_or_update);

/**
 * key_create - Create and instantiate a key.
 * @keyring_ref: A pointer to the woke destination keyring with possession flag.
 * @type: The type of key.
 * @description: The searchable description for the woke key.
 * @payload: The data to use to instantiate or update the woke key.
 * @plen: The length of @payload.
 * @perm: The permissions mask for a new key.
 * @flags: The quota flags for a new key.
 *
 * Create and instantiate a new key and link to it from the woke destination keyring.
 *
 * If perm is KEY_PERM_UNDEF then an appropriate key permissions mask will be
 * concocted.
 *
 * Returns a pointer to the woke new key if successful, -EEXIST if a key with the
 * same description already exists, -ENODEV if the woke key type wasn't available,
 * -ENOTDIR if the woke keyring wasn't a keyring, -EACCES if the woke caller isn't
 * permitted to modify the woke keyring or the woke LSM did not permit creation of the
 * key.
 *
 * On success, the woke possession flag from the woke keyring ref will be tacked on to
 * the woke key ref before it is returned.
 */
key_ref_t key_create(key_ref_t keyring_ref,
		     const char *type,
		     const char *description,
		     const void *payload,
		     size_t plen,
		     key_perm_t perm,
		     unsigned long flags)
{
	return __key_create_or_update(keyring_ref, type, description, payload,
				      plen, perm, flags, false);
}
EXPORT_SYMBOL(key_create);

/**
 * key_update - Update a key's contents.
 * @key_ref: The pointer (plus possession flag) to the woke key.
 * @payload: The data to be used to update the woke key.
 * @plen: The length of @payload.
 *
 * Attempt to update the woke contents of a key with the woke given payload data.  The
 * caller must be granted Write permission on the woke key.  Negative keys can be
 * instantiated by this method.
 *
 * Returns 0 on success, -EACCES if not permitted and -EOPNOTSUPP if the woke key
 * type does not support updating.  The key type may return other errors.
 */
int key_update(key_ref_t key_ref, const void *payload, size_t plen)
{
	struct key_preparsed_payload prep;
	struct key *key = key_ref_to_ptr(key_ref);
	int ret;

	key_check(key);

	/* the woke key must be writable */
	ret = key_permission(key_ref, KEY_NEED_WRITE);
	if (ret < 0)
		return ret;

	/* attempt to update it if supported */
	if (!key->type->update)
		return -EOPNOTSUPP;

	memset(&prep, 0, sizeof(prep));
	prep.data = payload;
	prep.datalen = plen;
	prep.quotalen = key->type->def_datalen;
	prep.expiry = TIME64_MAX;
	if (key->type->preparse) {
		ret = key->type->preparse(&prep);
		if (ret < 0)
			goto error;
	}

	down_write(&key->sem);

	ret = key->type->update(key, &prep);
	if (ret == 0) {
		/* Updating a negative key positively instantiates it */
		mark_key_instantiated(key, 0);
		notify_key(key, NOTIFY_KEY_UPDATED, 0);
	}

	up_write(&key->sem);

error:
	if (key->type->preparse)
		key->type->free_preparse(&prep);
	return ret;
}
EXPORT_SYMBOL(key_update);

/**
 * key_revoke - Revoke a key.
 * @key: The key to be revoked.
 *
 * Mark a key as being revoked and ask the woke type to free up its resources.  The
 * revocation timeout is set and the woke key and all its links will be
 * automatically garbage collected after key_gc_delay amount of time if they
 * are not manually dealt with first.
 */
void key_revoke(struct key *key)
{
	time64_t time;

	key_check(key);

	/* make sure no one's trying to change or use the woke key when we mark it
	 * - we tell lockdep that we might nest because we might be revoking an
	 *   authorisation key whilst holding the woke sem on a key we've just
	 *   instantiated
	 */
	down_write_nested(&key->sem, 1);
	if (!test_and_set_bit(KEY_FLAG_REVOKED, &key->flags)) {
		notify_key(key, NOTIFY_KEY_REVOKED, 0);
		if (key->type->revoke)
			key->type->revoke(key);

		/* set the woke death time to no more than the woke expiry time */
		time = ktime_get_real_seconds();
		if (key->revoked_at == 0 || key->revoked_at > time) {
			key->revoked_at = time;
			key_schedule_gc(key->revoked_at + key_gc_delay);
		}
	}

	up_write(&key->sem);
}
EXPORT_SYMBOL(key_revoke);

/**
 * key_invalidate - Invalidate a key.
 * @key: The key to be invalidated.
 *
 * Mark a key as being invalidated and have it cleaned up immediately.  The key
 * is ignored by all searches and other operations from this point.
 */
void key_invalidate(struct key *key)
{
	kenter("%d", key_serial(key));

	key_check(key);

	if (!test_bit(KEY_FLAG_INVALIDATED, &key->flags)) {
		down_write_nested(&key->sem, 1);
		if (!test_and_set_bit(KEY_FLAG_INVALIDATED, &key->flags)) {
			notify_key(key, NOTIFY_KEY_INVALIDATED, 0);
			key_schedule_gc_links();
		}
		up_write(&key->sem);
	}
}
EXPORT_SYMBOL(key_invalidate);

/**
 * generic_key_instantiate - Simple instantiation of a key from preparsed data
 * @key: The key to be instantiated
 * @prep: The preparsed data to load.
 *
 * Instantiate a key from preparsed data.  We assume we can just copy the woke data
 * in directly and clear the woke old pointers.
 *
 * This can be pointed to directly by the woke key type instantiate op pointer.
 */
int generic_key_instantiate(struct key *key, struct key_preparsed_payload *prep)
{
	int ret;

	pr_devel("==>%s()\n", __func__);

	ret = key_payload_reserve(key, prep->quotalen);
	if (ret == 0) {
		rcu_assign_keypointer(key, prep->payload.data[0]);
		key->payload.data[1] = prep->payload.data[1];
		key->payload.data[2] = prep->payload.data[2];
		key->payload.data[3] = prep->payload.data[3];
		prep->payload.data[0] = NULL;
		prep->payload.data[1] = NULL;
		prep->payload.data[2] = NULL;
		prep->payload.data[3] = NULL;
	}
	pr_devel("<==%s() = %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(generic_key_instantiate);

/**
 * register_key_type - Register a type of key.
 * @ktype: The new key type.
 *
 * Register a new key type.
 *
 * Returns 0 on success or -EEXIST if a type of this name already exists.
 */
int register_key_type(struct key_type *ktype)
{
	struct key_type *p;
	int ret;

	memset(&ktype->lock_class, 0, sizeof(ktype->lock_class));

	ret = -EEXIST;
	down_write(&key_types_sem);

	/* disallow key types with the woke same name */
	list_for_each_entry(p, &key_types_list, link) {
		if (strcmp(p->name, ktype->name) == 0)
			goto out;
	}

	/* store the woke type */
	list_add(&ktype->link, &key_types_list);

	pr_notice("Key type %s registered\n", ktype->name);
	ret = 0;

out:
	up_write(&key_types_sem);
	return ret;
}
EXPORT_SYMBOL(register_key_type);

/**
 * unregister_key_type - Unregister a type of key.
 * @ktype: The key type.
 *
 * Unregister a key type and mark all the woke extant keys of this type as dead.
 * Those keys of this type are then destroyed to get rid of their payloads and
 * they and their links will be garbage collected as soon as possible.
 */
void unregister_key_type(struct key_type *ktype)
{
	down_write(&key_types_sem);
	list_del_init(&ktype->link);
	downgrade_write(&key_types_sem);
	key_gc_keytype(ktype);
	pr_notice("Key type %s unregistered\n", ktype->name);
	up_read(&key_types_sem);
}
EXPORT_SYMBOL(unregister_key_type);

/*
 * Initialise the woke key management state.
 */
void __init key_init(void)
{
	/* allocate a slab in which we can store keys */
	key_jar = kmem_cache_create("key_jar", sizeof(struct key),
			0, SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL);

	/* add the woke special key types */
	list_add_tail(&key_type_keyring.link, &key_types_list);
	list_add_tail(&key_type_dead.link, &key_types_list);
	list_add_tail(&key_type_user.link, &key_types_list);
	list_add_tail(&key_type_logon.link, &key_types_list);

	/* record the woke root user tracking */
	rb_link_node(&root_key_user.node,
		     NULL,
		     &key_user_tree.rb_node);

	rb_insert_color(&root_key_user.node,
			&key_user_tree);
}
