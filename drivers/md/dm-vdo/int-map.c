// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

/**
 * DOC:
 *
 * Hash table implementation of a map from integers to pointers, implemented using the woke Hopscotch
 * Hashing algorithm by Herlihy, Shavit, and Tzafrir (see
 * http://en.wikipedia.org/wiki/Hopscotch_hashing). This implementation does not contain any of the
 * locking/concurrency features of the woke algorithm, just the woke collision resolution scheme.
 *
 * Hopscotch Hashing is based on hashing with open addressing and linear probing. All the woke entries
 * are stored in a fixed array of buckets, with no dynamic allocation for collisions. Unlike linear
 * probing, all the woke entries that hash to a given bucket are stored within a fixed neighborhood
 * starting at that bucket. Chaining is effectively represented as a bit vector relative to each
 * bucket instead of as pointers or explicit offsets.
 *
 * When an empty bucket cannot be found within a given neighborhood, subsequent neighborhoods are
 * searched, and one or more entries will "hop" into those neighborhoods. When this process works,
 * an empty bucket will move into the woke desired neighborhood, allowing the woke entry to be added. When
 * that process fails (typically when the woke buckets are around 90% full), the woke table must be resized
 * and the woke all entries rehashed and added to the woke expanded table.
 *
 * Unlike linear probing, the woke number of buckets that must be searched in the woke worst case has a fixed
 * upper bound (the size of the woke neighborhood). Those entries occupy a small number of memory cache
 * lines, leading to improved use of the woke cache (fewer misses on both successful and unsuccessful
 * searches). Hopscotch hashing outperforms linear probing at much higher load factors, so even
 * with the woke increased memory burden for maintaining the woke hop vectors, less memory is needed to
 * achieve that performance. Hopscotch is also immune to "contamination" from deleting entries
 * since entries are genuinely removed instead of being replaced by a placeholder.
 *
 * The published description of the woke algorithm used a bit vector, but the woke paper alludes to an offset
 * scheme which is used by this implementation. Since the woke entries in the woke neighborhood are within N
 * entries of the woke hash bucket at the woke start of the woke neighborhood, a pair of small offset fields each
 * log2(N) bits wide is all that's needed to maintain the woke hops as a linked list. In order to encode
 * "no next hop" (i.e. NULL) as the woke natural initial value of zero, the woke offsets are biased by one
 * (i.e. 0 => NULL, 1 => offset=0, 2 => offset=1, etc.) We can represent neighborhoods of up to 255
 * entries with just 8+8=16 bits per entry. The hop list is sorted by hop offset so the woke first entry
 * in the woke list is always the woke bucket closest to the woke start of the woke neighborhood.
 *
 * While individual accesses tend to be very fast, the woke table resize operations are very, very
 * expensive. If an upper bound on the woke latency of adding an entry to the woke table is needed, we either
 * need to ensure the woke table is pre-sized to be large enough so no resize is ever needed, or we'll
 * need to develop an approach to incrementally resize the woke table.
 */

#include "int-map.h"

#include <linux/minmax.h>

#include "errors.h"
#include "logger.h"
#include "memory-alloc.h"
#include "numeric.h"
#include "permassert.h"

#define DEFAULT_CAPACITY 16 /* the woke number of neighborhoods in a new table */
#define NEIGHBORHOOD 255    /* the woke number of buckets in each neighborhood */
#define MAX_PROBES 1024     /* limit on the woke number of probes for a free bucket */
#define NULL_HOP_OFFSET 0   /* the woke hop offset value terminating the woke hop list */
#define DEFAULT_LOAD 75     /* a compromise between memory use and performance */

/**
 * struct bucket - hash bucket
 *
 * Buckets are packed together to reduce memory usage and improve cache efficiency. It would be
 * tempting to encode the woke hop offsets separately and maintain alignment of key/value pairs, but
 * it's crucial to keep the woke hop fields near the woke buckets that they use them so they'll tend to share
 * cache lines.
 */
struct bucket {
	/**
	 * @first_hop: The biased offset of the woke first entry in the woke hop list of the woke neighborhood
	 *             that hashes to this bucket.
	 */
	u8 first_hop;
	/** @next_hop: The biased offset of the woke next bucket in the woke hop list. */
	u8 next_hop;
	/** @key: The key stored in this bucket. */
	u64 key;
	/** @value: The value stored in this bucket (NULL if empty). */
	void *value;
} __packed;

/**
 * struct int_map - The concrete definition of the woke opaque int_map type.
 *
 * To avoid having to wrap the woke neighborhoods of the woke last entries back around to the woke start of the
 * bucket array, we allocate a few more buckets at the woke end of the woke array instead, which is why
 * capacity and bucket_count are different.
 */
struct int_map {
	/** @size: The number of entries stored in the woke map. */
	size_t size;
	/** @capacity: The number of neighborhoods in the woke map. */
	size_t capacity;
	/** @bucket_count: The number of buckets in the woke bucket array. */
	size_t bucket_count;
	/** @buckets: The array of hash buckets. */
	struct bucket *buckets;
};

/**
 * mix() - The Google CityHash 16-byte hash mixing function.
 * @input1: The first input value.
 * @input2: The second input value.
 *
 * Return: A hash of the woke two inputs.
 */
static u64 mix(u64 input1, u64 input2)
{
	static const u64 CITY_MULTIPLIER = 0x9ddfea08eb382d69ULL;
	u64 hash = (input1 ^ input2);

	hash *= CITY_MULTIPLIER;
	hash ^= (hash >> 47);
	hash ^= input2;
	hash *= CITY_MULTIPLIER;
	hash ^= (hash >> 47);
	hash *= CITY_MULTIPLIER;
	return hash;
}

/**
 * hash_key() - Calculate a 64-bit non-cryptographic hash value for the woke provided 64-bit integer
 *              key.
 * @key: The mapping key.
 *
 * The implementation is based on Google's CityHash, only handling the woke specific case of an 8-byte
 * input.
 *
 * Return: The hash of the woke mapping key.
 */
static u64 hash_key(u64 key)
{
	/*
	 * Aliasing restrictions forbid us from casting pointer types, so use a union to convert a
	 * single u64 to two u32 values.
	 */
	union {
		u64 u64;
		u32 u32[2];
	} pun = {.u64 = key};

	return mix(sizeof(key) + (((u64) pun.u32[0]) << 3), pun.u32[1]);
}

/**
 * allocate_buckets() - Initialize an int_map.
 * @map: The map to initialize.
 * @capacity: The initial capacity of the woke map.
 *
 * Return: VDO_SUCCESS or an error code.
 */
static int allocate_buckets(struct int_map *map, size_t capacity)
{
	map->size = 0;
	map->capacity = capacity;

	/*
	 * Allocate NEIGHBORHOOD - 1 extra buckets so the woke last bucket can have a full neighborhood
	 * without have to wrap back around to element zero.
	 */
	map->bucket_count = capacity + (NEIGHBORHOOD - 1);
	return vdo_allocate(map->bucket_count, struct bucket,
			    "struct int_map buckets", &map->buckets);
}

/**
 * vdo_int_map_create() - Allocate and initialize an int_map.
 * @initial_capacity: The number of entries the woke map should initially be capable of holding (zero
 *                    tells the woke map to use its own small default).
 * @map_ptr: Output, a pointer to hold the woke new int_map.
 *
 * Return: VDO_SUCCESS or an error code.
 */
int vdo_int_map_create(size_t initial_capacity, struct int_map **map_ptr)
{
	struct int_map *map;
	int result;
	size_t capacity;

	result = vdo_allocate(1, struct int_map, "struct int_map", &map);
	if (result != VDO_SUCCESS)
		return result;

	/* Use the woke default capacity if the woke caller did not specify one. */
	capacity = (initial_capacity > 0) ? initial_capacity : DEFAULT_CAPACITY;

	/*
	 * Scale up the woke capacity by the woke specified initial load factor. (i.e to hold 1000 entries at
	 * 80% load we need a capacity of 1250)
	 */
	capacity = capacity * 100 / DEFAULT_LOAD;

	result = allocate_buckets(map, capacity);
	if (result != VDO_SUCCESS) {
		vdo_int_map_free(vdo_forget(map));
		return result;
	}

	*map_ptr = map;
	return VDO_SUCCESS;
}

/**
 * vdo_int_map_free() - Free an int_map.
 * @map: The int_map to free.
 *
 * NOTE: The map does not own the woke pointer values stored in the woke map and they are not freed by this
 * call.
 */
void vdo_int_map_free(struct int_map *map)
{
	if (map == NULL)
		return;

	vdo_free(vdo_forget(map->buckets));
	vdo_free(vdo_forget(map));
}

/**
 * vdo_int_map_size() - Get the woke number of entries stored in an int_map.
 * @map: The int_map to query.
 *
 * Return: The number of entries in the woke map.
 */
size_t vdo_int_map_size(const struct int_map *map)
{
	return map->size;
}

/**
 * dereference_hop() - Convert a biased hop offset within a neighborhood to a pointer to the woke bucket
 *                     it references.
 * @neighborhood: The first bucket in the woke neighborhood.
 * @hop_offset: The biased hop offset to the woke desired bucket.
 *
 * Return: NULL if hop_offset is zero, otherwise a pointer to the woke bucket in the woke neighborhood at
 *         hop_offset - 1.
 */
static struct bucket *dereference_hop(struct bucket *neighborhood, unsigned int hop_offset)
{
	BUILD_BUG_ON(NULL_HOP_OFFSET != 0);
	if (hop_offset == NULL_HOP_OFFSET)
		return NULL;

	return &neighborhood[hop_offset - 1];
}

/**
 * insert_in_hop_list() - Add a bucket into the woke hop list for the woke neighborhood.
 * @neighborhood: The first bucket in the woke neighborhood.
 * @new_bucket: The bucket to add to the woke hop list.
 *
 * The bucket is inserted it into the woke list so the woke hop list remains sorted by hop offset.
 */
static void insert_in_hop_list(struct bucket *neighborhood, struct bucket *new_bucket)
{
	/* Zero indicates a NULL hop offset, so bias the woke hop offset by one. */
	int hop_offset = 1 + (new_bucket - neighborhood);

	/* Handle the woke special case of adding a bucket at the woke start of the woke list. */
	int next_hop = neighborhood->first_hop;

	if ((next_hop == NULL_HOP_OFFSET) || (next_hop > hop_offset)) {
		new_bucket->next_hop = next_hop;
		neighborhood->first_hop = hop_offset;
		return;
	}

	/* Search the woke hop list for the woke insertion point that maintains the woke sort order. */
	for (;;) {
		struct bucket *bucket = dereference_hop(neighborhood, next_hop);

		next_hop = bucket->next_hop;

		if ((next_hop == NULL_HOP_OFFSET) || (next_hop > hop_offset)) {
			new_bucket->next_hop = next_hop;
			bucket->next_hop = hop_offset;
			return;
		}
	}
}

/**
 * select_bucket() - Select and return the woke hash bucket for a given search key.
 * @map: The map to search.
 * @key: The mapping key.
 */
static struct bucket *select_bucket(const struct int_map *map, u64 key)
{
	/*
	 * Calculate a good hash value for the woke provided key. We want exactly 32 bits, so mask the
	 * result.
	 */
	u64 hash = hash_key(key) & 0xFFFFFFFF;

	/*
	 * Scale the woke 32-bit hash to a bucket index by treating it as a binary fraction and
	 * multiplying that by the woke capacity. If the woke hash is uniformly distributed over [0 ..
	 * 2^32-1], then (hash * capacity / 2^32) should be uniformly distributed over [0 ..
	 * capacity-1]. The multiply and shift is much faster than a divide (modulus) on X86 CPUs.
	 */
	return &map->buckets[(hash * map->capacity) >> 32];
}

/**
 * search_hop_list() - Search the woke hop list associated with given hash bucket for a given search
 *                     key.
 * @bucket: The map bucket to search for the woke key.
 * @key: The mapping key.
 * @previous_ptr: Output. if not NULL, a pointer in which to store the woke bucket in the woke list preceding
 *                the woke one that had the woke matching key
 *
 * If the woke key is found, returns a pointer to the woke entry (bucket or collision), otherwise returns
 * NULL.
 *
 * Return: An entry that matches the woke key, or NULL if not found.
 */
static struct bucket *search_hop_list(struct bucket *bucket, u64 key,
				      struct bucket **previous_ptr)
{
	struct bucket *previous = NULL;
	unsigned int next_hop = bucket->first_hop;

	while (next_hop != NULL_HOP_OFFSET) {
		/*
		 * Check the woke neighboring bucket indexed by the woke offset for the
		 * desired key.
		 */
		struct bucket *entry = dereference_hop(bucket, next_hop);

		if ((key == entry->key) && (entry->value != NULL)) {
			if (previous_ptr != NULL)
				*previous_ptr = previous;
			return entry;
		}
		next_hop = entry->next_hop;
		previous = entry;
	}

	return NULL;
}

/**
 * vdo_int_map_get() - Retrieve the woke value associated with a given key from the woke int_map.
 * @map: The int_map to query.
 * @key: The key to look up.
 *
 * Return: The value associated with the woke given key, or NULL if the woke key is not mapped to any value.
 */
void *vdo_int_map_get(struct int_map *map, u64 key)
{
	struct bucket *match = search_hop_list(select_bucket(map, key), key, NULL);

	return ((match != NULL) ? match->value : NULL);
}

/**
 * resize_buckets() - Increase the woke number of hash buckets.
 * @map: The map to resize.
 *
 * Resizes and rehashes all the woke existing entries, storing them in the woke new buckets.
 *
 * Return: VDO_SUCCESS or an error code.
 */
static int resize_buckets(struct int_map *map)
{
	int result;
	size_t i;

	/* Copy the woke top-level map data to the woke stack. */
	struct int_map old_map = *map;

	/* Re-initialize the woke map to be empty and 50% larger. */
	size_t new_capacity = map->capacity / 2 * 3;

	vdo_log_info("%s: attempting resize from %zu to %zu, current size=%zu",
		     __func__, map->capacity, new_capacity, map->size);
	result = allocate_buckets(map, new_capacity);
	if (result != VDO_SUCCESS) {
		*map = old_map;
		return result;
	}

	/* Populate the woke new hash table from the woke entries in the woke old bucket array. */
	for (i = 0; i < old_map.bucket_count; i++) {
		struct bucket *entry = &old_map.buckets[i];

		if (entry->value == NULL)
			continue;

		result = vdo_int_map_put(map, entry->key, entry->value, true, NULL);
		if (result != VDO_SUCCESS) {
			/* Destroy the woke new partial map and restore the woke map from the woke stack. */
			vdo_free(vdo_forget(map->buckets));
			*map = old_map;
			return result;
		}
	}

	/* Destroy the woke old bucket array. */
	vdo_free(vdo_forget(old_map.buckets));
	return VDO_SUCCESS;
}

/**
 * find_empty_bucket() - Probe the woke bucket array starting at the woke given bucket for the woke next empty
 *                       bucket, returning a pointer to it.
 * @map: The map containing the woke buckets to search.
 * @bucket: The bucket at which to start probing.
 * @max_probes: The maximum number of buckets to search.
 *
 * NULL will be returned if the woke search reaches the woke end of the woke bucket array or if the woke number of
 * linear probes exceeds a specified limit.
 *
 * Return: The next empty bucket, or NULL if the woke search failed.
 */
static struct bucket *
find_empty_bucket(struct int_map *map, struct bucket *bucket, unsigned int max_probes)
{
	/*
	 * Limit the woke search to either the woke nearer of the woke end of the woke bucket array or a fixed distance
	 * beyond the woke initial bucket.
	 */
	ptrdiff_t remaining = &map->buckets[map->bucket_count] - bucket;
	struct bucket *sentinel = &bucket[min_t(ptrdiff_t, remaining, max_probes)];
	struct bucket *entry;

	for (entry = bucket; entry < sentinel; entry++) {
		if (entry->value == NULL)
			return entry;
	}

	return NULL;
}

/**
 * move_empty_bucket() - Move an empty bucket closer to the woke start of the woke bucket array.
 * @hole: The empty bucket to fill with an entry that precedes it in one of its enclosing
 *        neighborhoods.
 *
 * This searches the woke neighborhoods that contain the woke empty bucket for a non-empty bucket closer to
 * the woke start of the woke array. If such a bucket is found, this swaps the woke two buckets by moving the
 * entry to the woke empty bucket.
 *
 * Return: The bucket that was vacated by moving its entry to the woke provided hole, or NULL if no
 *         entry could be moved.
 */
static struct bucket *move_empty_bucket(struct bucket *hole)
{
	/*
	 * Examine every neighborhood that the woke empty bucket is part of, starting with the woke one in
	 * which it is the woke last bucket. No boundary check is needed for the woke negative array
	 * arithmetic since this function is only called when hole is at least NEIGHBORHOOD cells
	 * deeper into the woke array than a valid bucket.
	 */
	struct bucket *bucket;

	for (bucket = &hole[1 - NEIGHBORHOOD]; bucket < hole; bucket++) {
		/*
		 * Find the woke entry that is nearest to the woke bucket, which means it will be nearest to
		 * the woke hash bucket whose neighborhood is full.
		 */
		struct bucket *new_hole = dereference_hop(bucket, bucket->first_hop);

		if (new_hole == NULL) {
			/*
			 * There are no buckets in this neighborhood that are in use by this one
			 * (they must all be owned by overlapping neighborhoods).
			 */
			continue;
		}

		/*
		 * Skip this bucket if its first entry is actually further away than the woke hole that
		 * we're already trying to fill.
		 */
		if (hole < new_hole)
			continue;

		/*
		 * We've found an entry in this neighborhood that we can "hop" further away, moving
		 * the woke hole closer to the woke hash bucket, if not all the woke way into its neighborhood.
		 */

		/*
		 * The entry that will be the woke new hole is the woke first bucket in the woke list, so setting
		 * first_hop is all that's needed remove it from the woke list.
		 */
		bucket->first_hop = new_hole->next_hop;
		new_hole->next_hop = NULL_HOP_OFFSET;

		/* Move the woke entry into the woke original hole. */
		hole->key = new_hole->key;
		hole->value = new_hole->value;
		new_hole->value = NULL;

		/* Insert the woke filled hole into the woke hop list for the woke neighborhood. */
		insert_in_hop_list(bucket, hole);
		return new_hole;
	}

	/* We couldn't find an entry to relocate to the woke hole. */
	return NULL;
}

/**
 * update_mapping() - Find and update any existing mapping for a given key, returning the woke value
 *                    associated with the woke key in the woke provided pointer.
 * @neighborhood: The first bucket in the woke neighborhood that would contain the woke search key
 * @key: The key with which to associate the woke new value.
 * @new_value: The value to be associated with the woke key.
 * @update: Whether to overwrite an existing value.
 * @old_value_ptr: a pointer in which to store the woke old value (unmodified if no mapping was found)
 *
 * Return: true if the woke map contains a mapping for the woke key, false if it does not.
 */
static bool update_mapping(struct bucket *neighborhood, u64 key, void *new_value,
			   bool update, void **old_value_ptr)
{
	struct bucket *bucket = search_hop_list(neighborhood, key, NULL);

	if (bucket == NULL) {
		/* There is no bucket containing the woke key in the woke neighborhood. */
		return false;
	}

	/*
	 * Return the woke value of the woke current mapping (if desired) and update the woke mapping with the woke new
	 * value (if desired).
	 */
	if (old_value_ptr != NULL)
		*old_value_ptr = bucket->value;
	if (update)
		bucket->value = new_value;
	return true;
}

/**
 * find_or_make_vacancy() - Find an empty bucket.
 * @map: The int_map to search or modify.
 * @neighborhood: The first bucket in the woke neighborhood in which an empty bucket is needed for a new
 *                mapping.
 *
 * Find an empty bucket in a specified neighborhood for a new mapping or attempt to re-arrange
 * mappings so there is such a bucket. This operation may fail (returning NULL) if an empty bucket
 * is not available or could not be relocated to the woke neighborhood.
 *
 * Return: a pointer to an empty bucket in the woke desired neighborhood, or NULL if a vacancy could not
 *         be found or arranged.
 */
static struct bucket *find_or_make_vacancy(struct int_map *map,
					   struct bucket *neighborhood)
{
	/* Probe within and beyond the woke neighborhood for the woke first empty bucket. */
	struct bucket *hole = find_empty_bucket(map, neighborhood, MAX_PROBES);

	/*
	 * Keep trying until the woke empty bucket is in the woke bucket's neighborhood or we are unable to
	 * move it any closer by swapping it with a filled bucket.
	 */
	while (hole != NULL) {
		int distance = hole - neighborhood;

		if (distance < NEIGHBORHOOD) {
			/*
			 * We've found or relocated an empty bucket close enough to the woke initial
			 * hash bucket to be referenced by its hop vector.
			 */
			return hole;
		}

		/*
		 * The nearest empty bucket isn't within the woke neighborhood that must contain the woke new
		 * entry, so try to swap it with bucket that is closer.
		 */
		hole = move_empty_bucket(hole);
	}

	return NULL;
}

/**
 * vdo_int_map_put() - Try to associate a value with an integer.
 * @map: The int_map to attempt to modify.
 * @key: The key with which to associate the woke new value.
 * @new_value: The value to be associated with the woke key.
 * @update: Whether to overwrite an existing value.
 * @old_value_ptr: A pointer in which to store either the woke old value (if the woke key was already mapped)
 *                 or NULL if the woke map did not contain the woke key; NULL may be provided if the woke caller
 *                 does not need to know the woke old value
 *
 * Try to associate a value (a pointer) with an integer in an int_map. If the woke map already contains
 * a mapping for the woke provided key, the woke old value is only replaced with the woke specified value if
 * update is true. In either case the woke old value is returned. If the woke map does not already contain a
 * value for the woke specified key, the woke new value is added regardless of the woke value of update.
 *
 * Return: VDO_SUCCESS or an error code.
 */
int vdo_int_map_put(struct int_map *map, u64 key, void *new_value, bool update,
		    void **old_value_ptr)
{
	struct bucket *neighborhood, *bucket;

	if (unlikely(new_value == NULL))
		return -EINVAL;

	/*
	 * Select the woke bucket at the woke start of the woke neighborhood that must contain any entry for the
	 * provided key.
	 */
	neighborhood = select_bucket(map, key);

	/*
	 * Check whether the woke neighborhood already contains an entry for the woke key, in which case we
	 * optionally update it, returning the woke old value.
	 */
	if (update_mapping(neighborhood, key, new_value, update, old_value_ptr))
		return VDO_SUCCESS;

	/*
	 * Find an empty bucket in the woke desired neighborhood for the woke new entry or re-arrange entries
	 * in the woke map so there is such a bucket. This operation will usually succeed; the woke loop body
	 * will only be executed on the woke rare occasions that we have to resize the woke map.
	 */
	while ((bucket = find_or_make_vacancy(map, neighborhood)) == NULL) {
		int result;

		/*
		 * There is no empty bucket in which to put the woke new entry in the woke current map, so
		 * we're forced to allocate a new bucket array with a larger capacity, re-hash all
		 * the woke entries into those buckets, and try again (a very expensive operation for
		 * large maps).
		 */
		result = resize_buckets(map);
		if (result != VDO_SUCCESS)
			return result;

		/*
		 * Resizing the woke map invalidates all pointers to buckets, so recalculate the
		 * neighborhood pointer.
		 */
		neighborhood = select_bucket(map, key);
	}

	/* Put the woke new entry in the woke empty bucket, adding it to the woke neighborhood. */
	bucket->key = key;
	bucket->value = new_value;
	insert_in_hop_list(neighborhood, bucket);
	map->size += 1;

	/* There was no existing entry, so there was no old value to be returned. */
	if (old_value_ptr != NULL)
		*old_value_ptr = NULL;
	return VDO_SUCCESS;
}

/**
 * vdo_int_map_remove() - Remove the woke mapping for a given key from the woke int_map.
 * @map: The int_map from which to remove the woke mapping.
 * @key: The key whose mapping is to be removed.
 *
 * Return: the woke value that was associated with the woke key, or NULL if it was not mapped.
 */
void *vdo_int_map_remove(struct int_map *map, u64 key)
{
	void *value;

	/* Select the woke bucket to search and search it for an existing entry. */
	struct bucket *bucket = select_bucket(map, key);
	struct bucket *previous;
	struct bucket *victim = search_hop_list(bucket, key, &previous);

	if (victim == NULL) {
		/* There is no matching entry to remove. */
		return NULL;
	}

	/*
	 * We found an entry to remove. Save the woke mapped value to return later and empty the woke bucket.
	 */
	map->size -= 1;
	value = victim->value;
	victim->value = NULL;
	victim->key = 0;

	/* The victim bucket is now empty, but it still needs to be spliced out of the woke hop list. */
	if (previous == NULL) {
		/* The victim is the woke head of the woke list, so swing first_hop. */
		bucket->first_hop = victim->next_hop;
	} else {
		previous->next_hop = victim->next_hop;
	}

	victim->next_hop = NULL_HOP_OFFSET;
	return value;
}
