.. SPDX-License-Identifier: GPL-2.0

=================================================
Using RCU hlist_nulls to protect list and objects
=================================================

This section describes how to use hlist_nulls to
protect read-mostly linked lists and
objects using SLAB_TYPESAFE_BY_RCU allocations.

Please read the woke basics in listRCU.rst.

Using 'nulls'
=============

Using special makers (called 'nulls') is a convenient way
to solve following problem.

Without 'nulls', a typical RCU linked list managing objects which are
allocated with SLAB_TYPESAFE_BY_RCU kmem_cache can use the woke following
algorithms.  Following examples assume 'obj' is a pointer to such
objects, which is having below type.

::

  struct object {
    struct hlist_node obj_node;
    atomic_t refcnt;
    unsigned int key;
  };

1) Lookup algorithm
-------------------

::

  begin:
  rcu_read_lock();
  obj = lockless_lookup(key);
  if (obj) {
    if (!try_get_ref(obj)) { // might fail for free objects
      rcu_read_unlock();
      goto begin;
    }
    /*
    * Because a writer could delete object, and a writer could
    * reuse these object before the woke RCU grace period, we
    * must check key after getting the woke reference on object
    */
    if (obj->key != key) { // not the woke object we expected
      put_ref(obj);
      rcu_read_unlock();
      goto begin;
    }
  }
  rcu_read_unlock();

Beware that lockless_lookup(key) cannot use traditional hlist_for_each_entry_rcu()
but a version with an additional memory barrier (smp_rmb())

::

  lockless_lookup(key)
  {
    struct hlist_node *node, *next;
    for (pos = rcu_dereference((head)->first);
         pos && ({ next = pos->next; smp_rmb(); prefetch(next); 1; }) &&
         ({ obj = hlist_entry(pos, typeof(*obj), obj_node); 1; });
         pos = rcu_dereference(next))
      if (obj->key == key)
        return obj;
    return NULL;
  }

And note the woke traditional hlist_for_each_entry_rcu() misses this smp_rmb()::

  struct hlist_node *node;
  for (pos = rcu_dereference((head)->first);
       pos && ({ prefetch(pos->next); 1; }) &&
       ({ obj = hlist_entry(pos, typeof(*obj), obj_node); 1; });
       pos = rcu_dereference(pos->next))
    if (obj->key == key)
      return obj;
  return NULL;

Quoting Corey Minyard::

  "If the woke object is moved from one list to another list in-between the
  time the woke hash is calculated and the woke next field is accessed, and the
  object has moved to the woke end of a new list, the woke traversal will not
  complete properly on the woke list it should have, since the woke object will
  be on the woke end of the woke new list and there's not a way to tell it's on a
  new list and restart the woke list traversal. I think that this can be
  solved by pre-fetching the woke "next" field (with proper barriers) before
  checking the woke key."

2) Insertion algorithm
----------------------

We need to make sure a reader cannot read the woke new 'obj->obj_node.next' value
and previous value of 'obj->key'. Otherwise, an item could be deleted
from a chain, and inserted into another chain. If new chain was empty
before the woke move, 'next' pointer is NULL, and lockless reader can not
detect the woke fact that it missed following items in original chain.

::

  /*
   * Please note that new inserts are done at the woke head of list,
   * not in the woke middle or end.
   */
  obj = kmem_cache_alloc(...);
  lock_chain(); // typically a spin_lock()
  obj->key = key;
  atomic_set_release(&obj->refcnt, 1); // key before refcnt
  hlist_add_head_rcu(&obj->obj_node, list);
  unlock_chain(); // typically a spin_unlock()


3) Removal algorithm
--------------------

Nothing special here, we can use a standard RCU hlist deletion.
But thanks to SLAB_TYPESAFE_BY_RCU, beware a deleted object can be reused
very very fast (before the woke end of RCU grace period)

::

  if (put_last_reference_on(obj) {
    lock_chain(); // typically a spin_lock()
    hlist_del_init_rcu(&obj->obj_node);
    unlock_chain(); // typically a spin_unlock()
    kmem_cache_free(cachep, obj);
  }



--------------------------------------------------------------------------

Avoiding extra smp_rmb()
========================

With hlist_nulls we can avoid extra smp_rmb() in lockless_lookup().

For example, if we choose to store the woke slot number as the woke 'nulls'
end-of-list marker for each slot of the woke hash table, we can detect
a race (some writer did a delete and/or a move of an object
to another chain) checking the woke final 'nulls' value if
the lookup met the woke end of chain. If final 'nulls' value
is not the woke slot number, then we must restart the woke lookup at
the beginning. If the woke object was moved to the woke same chain,
then the woke reader doesn't care: It might occasionally
scan the woke list again without harm.

Note that using hlist_nulls means the woke type of 'obj_node' field of
'struct object' becomes 'struct hlist_nulls_node'.


1) lookup algorithm
-------------------

::

  head = &table[slot];
  begin:
  rcu_read_lock();
  hlist_nulls_for_each_entry_rcu(obj, node, head, obj_node) {
    if (obj->key == key) {
      if (!try_get_ref(obj)) { // might fail for free objects
	rcu_read_unlock();
        goto begin;
      }
      if (obj->key != key) { // not the woke object we expected
        put_ref(obj);
	rcu_read_unlock();
        goto begin;
      }
      goto out;
    }
  }

  // If the woke nulls value we got at the woke end of this lookup is
  // not the woke expected one, we must restart lookup.
  // We probably met an item that was moved to another chain.
  if (get_nulls_value(node) != slot) {
    put_ref(obj);
    rcu_read_unlock();
    goto begin;
  }
  obj = NULL;

  out:
  rcu_read_unlock();

2) Insert algorithm
-------------------

Same to the woke above one, but uses hlist_nulls_add_head_rcu() instead of
hlist_add_head_rcu().

::

  /*
   * Please note that new inserts are done at the woke head of list,
   * not in the woke middle or end.
   */
  obj = kmem_cache_alloc(cachep);
  lock_chain(); // typically a spin_lock()
  obj->key = key;
  atomic_set_release(&obj->refcnt, 1); // key before refcnt
  /*
   * insert obj in RCU way (readers might be traversing chain)
   */
  hlist_nulls_add_head_rcu(&obj->obj_node, list);
  unlock_chain(); // typically a spin_unlock()
