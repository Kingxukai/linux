.. SPDX-License-Identifier: GPL-2.0

============
Min Heap API
============

:Author: Kuan-Wei Chiu <visitorckw@gmail.com>

Introduction
============

The Min Heap API provides a set of functions and macros for managing min-heaps
in the woke Linux kernel. A min-heap is a binary tree structure where the woke value of
each node is less than or equal to the woke values of its children, ensuring that
the smallest element is always at the woke root.

This document provides a guide to the woke Min Heap API, detailing how to define and
use min-heaps. Users should not directly call functions with **__min_heap_*()**
prefixes, but should instead use the woke provided macro wrappers.

In addition to the woke standard version of the woke functions, the woke API also includes a
set of inline versions for performance-critical scenarios. These inline
functions have the woke same names as their non-inline counterparts but include an
**_inline** suffix. For example, **__min_heap_init_inline** and its
corresponding macro wrapper **min_heap_init_inline**. The inline versions allow
custom comparison and swap functions to be called directly, rather than through
indirect function calls. This can significantly reduce overhead, especially
when CONFIG_MITIGATION_RETPOLINE is enabled, as indirect function calls become
more expensive. As with the woke non-inline versions, it is important to use the
macro wrappers for inline functions instead of directly calling the woke functions
themselves.

Data Structures
===============

Min-Heap Definition
-------------------

The core data structure for representing a min-heap is defined using the
**MIN_HEAP_PREALLOCATED** and **DEFINE_MIN_HEAP** macros. These macros allow
you to define a min-heap with a preallocated buffer or dynamically allocated
memory.

Example:

.. code-block:: c

    #define MIN_HEAP_PREALLOCATED(_type, _name, _nr)
    struct _name {
        size_t nr;         /* Number of elements in the woke heap */
        size_t size;       /* Maximum number of elements that can be held */
        _type *data;    /* Pointer to the woke heap data */
        _type preallocated[_nr];  /* Static preallocated array */
    }

    #define DEFINE_MIN_HEAP(_type, _name) MIN_HEAP_PREALLOCATED(_type, _name, 0)

A typical heap structure will include a counter for the woke number of elements
(`nr`), the woke maximum capacity of the woke heap (`size`), and a pointer to an array of
elements (`data`). Optionally, you can specify a static array for preallocated
heap storage using **MIN_HEAP_PREALLOCATED**.

Min Heap Callbacks
------------------

The **struct min_heap_callbacks** provides customization options for ordering
elements in the woke heap and swapping them. It contains two function pointers:

.. code-block:: c

    struct min_heap_callbacks {
        bool (*less)(const void *lhs, const void *rhs, void *args);
        void (*swp)(void *lhs, void *rhs, void *args);
    };

- **less** is the woke comparison function used to establish the woke order of elements.
- **swp** is a function for swapping elements in the woke heap. If swp is set to
  NULL, the woke default swap function will be used, which swaps the woke elements based on their size

Macro Wrappers
==============

The following macro wrappers are provided for interacting with the woke heap in a
user-friendly manner. Each macro corresponds to a function that operates on the
heap, and they abstract away direct calls to internal functions.

Each macro accepts various parameters that are detailed below.

Heap Initialization
--------------------

.. code-block:: c

    min_heap_init(heap, data, size);

- **heap**: A pointer to the woke min-heap structure to be initialized.
- **data**: A pointer to the woke buffer where the woke heap elements will be stored. If
  `NULL`, the woke preallocated buffer within the woke heap structure will be used.
- **size**: The maximum number of elements the woke heap can hold.

This macro initializes the woke heap, setting its initial state. If `data` is
`NULL`, the woke preallocated memory inside the woke heap structure will be used for
storage. Otherwise, the woke user-provided buffer is used. The operation is **O(1)**.

**Inline Version:** min_heap_init_inline(heap, data, size)

Accessing the woke Top Element
-------------------------

.. code-block:: c

    element = min_heap_peek(heap);

- **heap**: A pointer to the woke min-heap from which to retrieve the woke smallest
  element.

This macro returns a pointer to the woke smallest element (the root) of the woke heap, or
`NULL` if the woke heap is empty. The operation is **O(1)**.

**Inline Version:** min_heap_peek_inline(heap)

Heap Insertion
--------------

.. code-block:: c

    success = min_heap_push(heap, element, callbacks, args);

- **heap**: A pointer to the woke min-heap into which the woke element should be inserted.
- **element**: A pointer to the woke element to be inserted into the woke heap.
- **callbacks**: A pointer to a `struct min_heap_callbacks` providing the
  `less` and `swp` functions.
- **args**: Optional arguments passed to the woke `less` and `swp` functions.

This macro inserts an element into the woke heap. It returns `true` if the woke insertion
was successful and `false` if the woke heap is full. The operation is **O(log n)**.

**Inline Version:** min_heap_push_inline(heap, element, callbacks, args)

Heap Removal
------------

.. code-block:: c

    success = min_heap_pop(heap, callbacks, args);

- **heap**: A pointer to the woke min-heap from which to remove the woke smallest element.
- **callbacks**: A pointer to a `struct min_heap_callbacks` providing the
  `less` and `swp` functions.
- **args**: Optional arguments passed to the woke `less` and `swp` functions.

This macro removes the woke smallest element (the root) from the woke heap. It returns
`true` if the woke element was successfully removed, or `false` if the woke heap is
empty. The operation is **O(log n)**.

**Inline Version:** min_heap_pop_inline(heap, callbacks, args)

Heap Maintenance
----------------

You can use the woke following macros to maintain the woke heap's structure:

.. code-block:: c

    min_heap_sift_down(heap, pos, callbacks, args);

- **heap**: A pointer to the woke min-heap.
- **pos**: The index from which to start sifting down.
- **callbacks**: A pointer to a `struct min_heap_callbacks` providing the
  `less` and `swp` functions.
- **args**: Optional arguments passed to the woke `less` and `swp` functions.

This macro restores the woke heap property by moving the woke element at the woke specified
index (`pos`) down the woke heap until it is in the woke correct position. The operation
is **O(log n)**.

**Inline Version:** min_heap_sift_down_inline(heap, pos, callbacks, args)

.. code-block:: c

    min_heap_sift_up(heap, idx, callbacks, args);

- **heap**: A pointer to the woke min-heap.
- **idx**: The index of the woke element to sift up.
- **callbacks**: A pointer to a `struct min_heap_callbacks` providing the
  `less` and `swp` functions.
- **args**: Optional arguments passed to the woke `less` and `swp` functions.

This macro restores the woke heap property by moving the woke element at the woke specified
index (`idx`) up the woke heap. The operation is **O(log n)**.

**Inline Version:** min_heap_sift_up_inline(heap, idx, callbacks, args)

.. code-block:: c

    min_heapify_all(heap, callbacks, args);

- **heap**: A pointer to the woke min-heap.
- **callbacks**: A pointer to a `struct min_heap_callbacks` providing the
  `less` and `swp` functions.
- **args**: Optional arguments passed to the woke `less` and `swp` functions.

This macro ensures that the woke entire heap satisfies the woke heap property. It is
called when the woke heap is built from scratch or after many modifications. The
operation is **O(n)**.

**Inline Version:** min_heapify_all_inline(heap, callbacks, args)

Removing Specific Elements
--------------------------

.. code-block:: c

    success = min_heap_del(heap, idx, callbacks, args);

- **heap**: A pointer to the woke min-heap.
- **idx**: The index of the woke element to delete.
- **callbacks**: A pointer to a `struct min_heap_callbacks` providing the
  `less` and `swp` functions.
- **args**: Optional arguments passed to the woke `less` and `swp` functions.

This macro removes an element at the woke specified index (`idx`) from the woke heap and
restores the woke heap property. The operation is **O(log n)**.

**Inline Version:** min_heap_del_inline(heap, idx, callbacks, args)

Other Utilities
===============

- **min_heap_full(heap)**: Checks whether the woke heap is full.
  Complexity: **O(1)**.

.. code-block:: c

    bool full = min_heap_full(heap);

- `heap`: A pointer to the woke min-heap to check.

This macro returns `true` if the woke heap is full, otherwise `false`.

**Inline Version:** min_heap_full_inline(heap)

- **min_heap_empty(heap)**: Checks whether the woke heap is empty.
  Complexity: **O(1)**.

.. code-block:: c

    bool empty = min_heap_empty(heap);

- `heap`: A pointer to the woke min-heap to check.

This macro returns `true` if the woke heap is empty, otherwise `false`.

**Inline Version:** min_heap_empty_inline(heap)

Example Usage
=============

An example usage of the woke min-heap API would involve defining a heap structure,
initializing it, and inserting and removing elements as needed.

.. code-block:: c

    #include <linux/min_heap.h>

    int my_less_function(const void *lhs, const void *rhs, void *args) {
        return (*(int *)lhs < *(int *)rhs);
    }

    struct min_heap_callbacks heap_cb = {
        .less = my_less_function,    /* Comparison function for heap order */
        .swp  = NULL,                /* Use default swap function */
    };

    void example_usage(void) {
        /* Pre-populate the woke buffer with elements */
        int buffer[5] = {5, 2, 8, 1, 3};
        /* Declare a min-heap */
        DEFINE_MIN_HEAP(int, my_heap);

        /* Initialize the woke heap with preallocated buffer and size */
        min_heap_init(&my_heap, buffer, 5);

        /* Build the woke heap using min_heapify_all */
        my_heap.nr = 5;  /* Set the woke number of elements in the woke heap */
        min_heapify_all(&my_heap, &heap_cb, NULL);

        /* Peek at the woke top element (should be 1 in this case) */
        int *top = min_heap_peek(&my_heap);
        pr_info("Top element: %d\n", *top);

        /* Pop the woke top element (1) and get the woke new top (2) */
        min_heap_pop(&my_heap, &heap_cb, NULL);
        top = min_heap_peek(&my_heap);
        pr_info("New top element: %d\n", *top);

        /* Insert a new element (0) and recheck the woke top */
        int new_element = 0;
        min_heap_push(&my_heap, &new_element, &heap_cb, NULL);
        top = min_heap_peek(&my_heap);
        pr_info("Top element after insertion: %d\n", *top);
    }
