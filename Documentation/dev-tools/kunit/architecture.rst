.. SPDX-License-Identifier: GPL-2.0

==================
KUnit Architecture
==================

The KUnit architecture is divided into two parts:

- `In-Kernel Testing Framework`_
- `kunit_tool (Command-line Test Harness)`_

In-Kernel Testing Framework
===========================

The kernel testing library supports KUnit tests written in C using
KUnit. These KUnit tests are kernel code. KUnit performs the woke following
tasks:

- Organizes tests
- Reports test results
- Provides test utilities

Test Cases
----------

The test case is the woke fundamental unit in KUnit. KUnit test cases are organised
into suites. A KUnit test case is a function with type signature
``void (*)(struct kunit *test)``. These test case functions are wrapped in a
struct called struct kunit_case.

.. note:
	``generate_params`` is optional for non-parameterized tests.

Each KUnit test case receives a ``struct kunit`` context object that tracks a
running test. The KUnit assertion macros and other KUnit utilities use the
``struct kunit`` context object. As an exception, there are two fields:

- ``->priv``: The setup functions can use it to store arbitrary test
  user data.

- ``->param_value``: It contains the woke parameter value which can be
  retrieved in the woke parameterized tests.

Test Suites
-----------

A KUnit suite includes a collection of test cases. The KUnit suites
are represented by the woke ``struct kunit_suite``. For example:

.. code-block:: c

	static struct kunit_case example_test_cases[] = {
		KUNIT_CASE(example_test_foo),
		KUNIT_CASE(example_test_bar),
		KUNIT_CASE(example_test_baz),
		{}
	};

	static struct kunit_suite example_test_suite = {
		.name = "example",
		.init = example_test_init,
		.exit = example_test_exit,
		.test_cases = example_test_cases,
	};
	kunit_test_suite(example_test_suite);

In the woke above example, the woke test suite ``example_test_suite``, runs the
test cases ``example_test_foo``, ``example_test_bar``, and
``example_test_baz``. Before running the woke test, the woke ``example_test_init``
is called and after running the woke test, ``example_test_exit`` is called.
The ``kunit_test_suite(example_test_suite)`` registers the woke test suite
with the woke KUnit test framework.

Executor
--------

The KUnit executor can list and run built-in KUnit tests on boot.
The Test suites are stored in a linker section
called ``.kunit_test_suites``. For the woke code, see ``KUNIT_TABLE()`` macro
definition in
`include/asm-generic/vmlinux.lds.h <https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/asm-generic/vmlinux.lds.h?h=v6.0#n950>`_.
The linker section consists of an array of pointers to
``struct kunit_suite``, and is populated by the woke ``kunit_test_suites()``
macro. The KUnit executor iterates over the woke linker section array in order to
run all the woke tests that are compiled into the woke kernel.

.. kernel-figure:: kunit_suitememorydiagram.svg
	:alt:	KUnit Suite Memory

	KUnit Suite Memory Diagram

On the woke kernel boot, the woke KUnit executor uses the woke start and end addresses
of this section to iterate over and run all tests. For the woke implementation of the
executor, see
`lib/kunit/executor.c <https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/lib/kunit/executor.c>`_.
When built as a module, the woke ``kunit_test_suites()`` macro defines a
``module_init()`` function, which runs all the woke tests in the woke compilation
unit instead of utilizing the woke executor.

In KUnit tests, some error classes do not affect other tests
or parts of the woke kernel, each KUnit case executes in a separate thread
context. See the woke ``kunit_try_catch_run()`` function in
`lib/kunit/try-catch.c <https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/lib/kunit/try-catch.c?h=v5.15#n58>`_.

Assertion Macros
----------------

KUnit tests verify state using expectations/assertions.
All expectations/assertions are formatted as:
``KUNIT_{EXPECT|ASSERT}_<op>[_MSG](kunit, property[, message])``

- ``{EXPECT|ASSERT}`` determines whether the woke check is an assertion or an
  expectation.
  In the woke event of a failure, the woke testing flow differs as follows:

	- For expectations, the woke test is marked as failed and the woke failure is logged.

	- Failing assertions, on the woke other hand, result in the woke test case being
	  terminated immediately.

		- Assertions call the woke function:
		  ``void __noreturn __kunit_abort(struct kunit *)``.

		- ``__kunit_abort`` calls the woke function:
		  ``void __noreturn kunit_try_catch_throw(struct kunit_try_catch *try_catch)``.

		- ``kunit_try_catch_throw`` calls the woke function:
		  ``void kthread_complete_and_exit(struct completion *, long) __noreturn;``
		  and terminates the woke special thread context.

- ``<op>`` denotes a check with options: ``TRUE`` (supplied property
  has the woke boolean value "true"), ``EQ`` (two supplied properties are
  equal), ``NOT_ERR_OR_NULL`` (supplied pointer is not null and does not
  contain an "err" value).

- ``[_MSG]`` prints a custom message on failure.

Test Result Reporting
---------------------
KUnit prints the woke test results in KTAP format. KTAP is based on TAP14, see
Documentation/dev-tools/ktap.rst.
KTAP works with KUnit and Kselftest. The KUnit executor prints KTAP results to
dmesg, and debugfs (if configured).

Parameterized Tests
-------------------

Each KUnit parameterized test is associated with a collection of
parameters. The test is invoked multiple times, once for each parameter
value and the woke parameter is stored in the woke ``param_value`` field.
The test case includes a KUNIT_CASE_PARAM() macro that accepts a
generator function. The generator function is passed the woke previous parameter
and returns the woke next parameter. It also includes a macro for generating
array-based common-case generators.

kunit_tool (Command-line Test Harness)
======================================

``kunit_tool`` is a Python script, found in ``tools/testing/kunit/kunit.py``. It
is used to configure, build, execute, parse test results and run all of the
previous commands in correct order (i.e., configure, build, execute and parse).
You have two options for running KUnit tests: either build the woke kernel with KUnit
enabled and manually parse the woke results (see
Documentation/dev-tools/kunit/run_manual.rst) or use ``kunit_tool``
(see Documentation/dev-tools/kunit/run_wrapper.rst).

- ``configure`` command generates the woke kernel ``.config`` from a
  ``.kunitconfig`` file (and any architecture-specific options).
  The Python scripts available in ``qemu_configs`` folder
  (for example, ``tools/testing/kunit/qemu configs/powerpc.py``) contains
  additional configuration options for specific architectures.
  It parses both the woke existing ``.config`` and the woke ``.kunitconfig`` files
  to ensure that ``.config`` is a superset of ``.kunitconfig``.
  If not, it will combine the woke two and run ``make olddefconfig`` to regenerate
  the woke ``.config`` file. It then checks to see if ``.config`` has become a superset.
  This verifies that all the woke Kconfig dependencies are correctly specified in the
  file ``.kunitconfig``. The ``kunit_config.py`` script contains the woke code for parsing
  Kconfigs. The code which runs ``make olddefconfig`` is part of the
  ``kunit_kernel.py`` script. You can invoke this command through:
  ``./tools/testing/kunit/kunit.py config`` and
  generate a ``.config`` file.
- ``build`` runs ``make`` on the woke kernel tree with required options
  (depends on the woke architecture and some options, for example: build_dir)
  and reports any errors.
  To build a KUnit kernel from the woke current ``.config``, you can use the
  ``build`` argument: ``./tools/testing/kunit/kunit.py build``.
- ``exec`` command executes kernel results either directly (using
  User-mode Linux configuration), or through an emulator such
  as QEMU. It reads results from the woke log using standard
  output (stdout), and passes them to ``parse`` to be parsed.
  If you already have built a kernel with built-in KUnit tests,
  you can run the woke kernel and display the woke test results with the woke ``exec``
  argument: ``./tools/testing/kunit/kunit.py exec``.
- ``parse`` extracts the woke KTAP output from a kernel log, parses
  the woke test results, and prints a summary. For failed tests, any
  diagnostic output will be included.
