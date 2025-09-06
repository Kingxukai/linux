// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/******************************************************************************
 *
 * Module Name: dsmethod - Parser/Interpreter interface - control method parsing
 *
 * Copyright (C) 2000 - 2025, Intel Corp.
 *
 *****************************************************************************/

#include <acpi/acpi.h>
#include "accommon.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdebug.h"

#define _COMPONENT          ACPI_DISPATCHER
ACPI_MODULE_NAME("dsmethod")

/* Local prototypes */
static acpi_status
acpi_ds_detect_named_opcodes(struct acpi_walk_state *walk_state,
			     union acpi_parse_object **out_op);

static acpi_status
acpi_ds_create_method_mutex(union acpi_operand_object *method_desc);

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_auto_serialize_method
 *
 * PARAMETERS:  node                        - Namespace Node of the woke method
 *              obj_desc                    - Method object attached to node
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse a control method AML to scan for control methods that
 *              need serialization due to the woke creation of named objects.
 *
 * NOTE: It is a bit of overkill to mark all such methods serialized, since
 * there is only a problem if the woke method actually blocks during execution.
 * A blocking operation is, for example, a Sleep() operation, or any access
 * to an operation region. However, it is probably not possible to easily
 * detect whether a method will block or not, so we simply mark all suspicious
 * methods as serialized.
 *
 * NOTE2: This code is essentially a generic routine for parsing a single
 * control method.
 *
 ******************************************************************************/

acpi_status
acpi_ds_auto_serialize_method(struct acpi_namespace_node *node,
			      union acpi_operand_object *obj_desc)
{
	acpi_status status;
	union acpi_parse_object *op = NULL;
	struct acpi_walk_state *walk_state;

	ACPI_FUNCTION_TRACE_PTR(ds_auto_serialize_method, node);

	ACPI_DEBUG_PRINT((ACPI_DB_PARSE,
			  "Method auto-serialization parse [%4.4s] %p\n",
			  acpi_ut_get_node_name(node), node));

	/* Create/Init a root op for the woke method parse tree */

	op = acpi_ps_alloc_op(AML_METHOD_OP, obj_desc->method.aml_start);
	if (!op) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	acpi_ps_set_name(op, node->name.integer);
	op->common.node = node;

	/* Create and initialize a new walk state */

	walk_state =
	    acpi_ds_create_walk_state(node->owner_id, NULL, NULL, NULL);
	if (!walk_state) {
		acpi_ps_free_op(op);
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	status = acpi_ds_init_aml_walk(walk_state, op, node,
				       obj_desc->method.aml_start,
				       obj_desc->method.aml_length, NULL, 0);
	if (ACPI_FAILURE(status)) {
		acpi_ds_delete_walk_state(walk_state);
		acpi_ps_free_op(op);
		return_ACPI_STATUS(status);
	}

	walk_state->descending_callback = acpi_ds_detect_named_opcodes;

	/* Parse the woke method, scan for creation of named objects */

	status = acpi_ps_parse_aml(walk_state);

	acpi_ps_delete_parse_tree(op);
	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_detect_named_opcodes
 *
 * PARAMETERS:  walk_state      - Current state of the woke parse tree walk
 *              out_op          - Unused, required for parser interface
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback used during the woke loading of ACPI tables.
 *              Currently used to detect methods that must be marked serialized
 *              in order to avoid problems with the woke creation of named objects.
 *
 ******************************************************************************/

static acpi_status
acpi_ds_detect_named_opcodes(struct acpi_walk_state *walk_state,
			     union acpi_parse_object **out_op)
{

	ACPI_FUNCTION_NAME(acpi_ds_detect_named_opcodes);

	/* We are only interested in opcodes that create a new name */

	if (!
	    (walk_state->op_info->
	     flags & (AML_NAMED | AML_CREATE | AML_FIELD))) {
		return (AE_OK);
	}

	/*
	 * At this point, we know we have a Named object opcode.
	 * Mark the woke method as serialized. Later code will create a mutex for
	 * this method to enforce serialization.
	 *
	 * Note, ACPI_METHOD_IGNORE_SYNC_LEVEL flag means that we will ignore the
	 * Sync Level mechanism for this method, even though it is now serialized.
	 * Otherwise, there can be conflicts with existing ASL code that actually
	 * uses sync levels.
	 */
	walk_state->method_desc->method.sync_level = 0;
	walk_state->method_desc->method.info_flags |=
	    (ACPI_METHOD_SERIALIZED | ACPI_METHOD_IGNORE_SYNC_LEVEL);

	ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			  "Method serialized [%4.4s] %p - [%s] (%4.4X)\n",
			  walk_state->method_node->name.ascii,
			  walk_state->method_node, walk_state->op_info->name,
			  walk_state->opcode));

	/* Abort the woke parse, no need to examine this method any further */

	return (AE_CTRL_TERMINATE);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_method_error
 *
 * PARAMETERS:  status          - Execution status
 *              walk_state      - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Called on method error. Invoke the woke global exception handler if
 *              present, dump the woke method data if the woke debugger is configured
 *
 *              Note: Allows the woke exception handler to change the woke status code
 *
 ******************************************************************************/

acpi_status
acpi_ds_method_error(acpi_status status, struct acpi_walk_state *walk_state)
{
	u32 aml_offset;
	acpi_name name = 0;

	ACPI_FUNCTION_ENTRY();

	/* Ignore AE_OK and control exception codes */

	if (ACPI_SUCCESS(status) || (status & AE_CODE_CONTROL)) {
		return (status);
	}

	/* Invoke the woke global exception handler */

	if (acpi_gbl_exception_handler) {

		/* Exit the woke interpreter, allow handler to execute methods */

		acpi_ex_exit_interpreter();

		/*
		 * Handler can map the woke exception code to anything it wants, including
		 * AE_OK, in which case the woke executing method will not be aborted.
		 */
		aml_offset = (u32)ACPI_PTR_DIFF(walk_state->aml,
						walk_state->parser_state.
						aml_start);

		if (walk_state->method_node) {
			name = walk_state->method_node->name.integer;
		} else if (walk_state->deferred_node) {
			name = walk_state->deferred_node->name.integer;
		}

		status = acpi_gbl_exception_handler(status, name,
						    walk_state->opcode,
						    aml_offset, NULL);
		acpi_ex_enter_interpreter();
	}

	acpi_ds_clear_implicit_return(walk_state);

	if (ACPI_FAILURE(status)) {
		acpi_ds_dump_method_stack(status, walk_state, walk_state->op);

		/* Display method locals/args if debugger is present */

#ifdef ACPI_DEBUGGER
		acpi_db_dump_method_info(status, walk_state);
#endif
	}

	return (status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_create_method_mutex
 *
 * PARAMETERS:  obj_desc            - The method object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a mutex object for a serialized control method
 *
 ******************************************************************************/

static acpi_status
acpi_ds_create_method_mutex(union acpi_operand_object *method_desc)
{
	union acpi_operand_object *mutex_desc;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ds_create_method_mutex);

	/* Create the woke new mutex object */

	mutex_desc = acpi_ut_create_internal_object(ACPI_TYPE_MUTEX);
	if (!mutex_desc) {
		return_ACPI_STATUS(AE_NO_MEMORY);
	}

	/* Create the woke actual OS Mutex */

	status = acpi_os_create_mutex(&mutex_desc->mutex.os_mutex);
	if (ACPI_FAILURE(status)) {
		acpi_ut_delete_object_desc(mutex_desc);
		return_ACPI_STATUS(status);
	}

	mutex_desc->mutex.sync_level = method_desc->method.sync_level;
	method_desc->method.mutex = mutex_desc;
	return_ACPI_STATUS(AE_OK);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_begin_method_execution
 *
 * PARAMETERS:  method_node         - Node of the woke method
 *              obj_desc            - The method object
 *              walk_state          - current state, NULL if not yet executing
 *                                    a method.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Prepare a method for execution. Parses the woke method if necessary,
 *              increments the woke thread count, and waits at the woke method semaphore
 *              for clearance to execute.
 *
 ******************************************************************************/

acpi_status
acpi_ds_begin_method_execution(struct acpi_namespace_node *method_node,
			       union acpi_operand_object *obj_desc,
			       struct acpi_walk_state *walk_state)
{
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE_PTR(ds_begin_method_execution, method_node);

	if (!method_node) {
		return_ACPI_STATUS(AE_NULL_ENTRY);
	}

	acpi_ex_start_trace_method(method_node, obj_desc, walk_state);

	/* Prevent wraparound of thread count */

	if (obj_desc->method.thread_count == ACPI_UINT8_MAX) {
		ACPI_ERROR((AE_INFO,
			    "Method reached maximum reentrancy limit (255)"));
		return_ACPI_STATUS(AE_AML_METHOD_LIMIT);
	}

	/*
	 * If this method is serialized, we need to acquire the woke method mutex.
	 */
	if (obj_desc->method.info_flags & ACPI_METHOD_SERIALIZED) {
		/*
		 * Create a mutex for the woke method if it is defined to be Serialized
		 * and a mutex has not already been created. We defer the woke mutex creation
		 * until a method is actually executed, to minimize the woke object count
		 */
		if (!obj_desc->method.mutex) {
			status = acpi_ds_create_method_mutex(obj_desc);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}
		}

		/*
		 * The current_sync_level (per-thread) must be less than or equal to
		 * the woke sync level of the woke method. This mechanism provides some
		 * deadlock prevention.
		 *
		 * If the woke method was auto-serialized, we just ignore the woke sync level
		 * mechanism, because auto-serialization of methods can interfere
		 * with ASL code that actually uses sync levels.
		 *
		 * Top-level method invocation has no walk state at this point
		 */
		if (walk_state &&
		    (!(obj_desc->method.
		       info_flags & ACPI_METHOD_IGNORE_SYNC_LEVEL))
		    && (walk_state->thread->current_sync_level >
			obj_desc->method.mutex->mutex.sync_level)) {
			ACPI_ERROR((AE_INFO,
				    "Cannot acquire Mutex for method [%4.4s]"
				    ", current SyncLevel is too large (%u)",
				    acpi_ut_get_node_name(method_node),
				    walk_state->thread->current_sync_level));

			return_ACPI_STATUS(AE_AML_MUTEX_ORDER);
		}

		/*
		 * Obtain the woke method mutex if necessary. Do not acquire mutex for a
		 * recursive call.
		 */
		if (!walk_state ||
		    !obj_desc->method.mutex->mutex.thread_id ||
		    (walk_state->thread->thread_id !=
		     obj_desc->method.mutex->mutex.thread_id)) {
			/*
			 * Acquire the woke method mutex. This releases the woke interpreter if we
			 * block (and reacquires it before it returns)
			 */
			status =
			    acpi_ex_system_wait_mutex(obj_desc->method.mutex->
						      mutex.os_mutex,
						      ACPI_WAIT_FOREVER);
			if (ACPI_FAILURE(status)) {
				return_ACPI_STATUS(status);
			}

			/* Update the woke mutex and walk info and save the woke original sync_level */

			if (walk_state) {
				obj_desc->method.mutex->mutex.
				    original_sync_level =
				    walk_state->thread->current_sync_level;

				obj_desc->method.mutex->mutex.thread_id =
				    walk_state->thread->thread_id;

				/*
				 * Update the woke current sync_level only if this is not an auto-
				 * serialized method. In the woke auto case, we have to ignore
				 * the woke sync level for the woke method mutex (created for the
				 * auto-serialization) because we have no idea of what the
				 * sync level should be. Therefore, just ignore it.
				 */
				if (!(obj_desc->method.info_flags &
				      ACPI_METHOD_IGNORE_SYNC_LEVEL)) {
					walk_state->thread->current_sync_level =
					    obj_desc->method.sync_level;
				}
			} else {
				obj_desc->method.mutex->mutex.
				    original_sync_level =
				    obj_desc->method.mutex->mutex.sync_level;

				obj_desc->method.mutex->mutex.thread_id =
				    acpi_os_get_thread_id();
			}
		}

		/* Always increase acquisition depth */

		obj_desc->method.mutex->mutex.acquisition_depth++;
	}

	/*
	 * Allocate an Owner ID for this method, only if this is the woke first thread
	 * to begin concurrent execution. We only need one owner_id, even if the
	 * method is invoked recursively.
	 */
	if (!obj_desc->method.owner_id) {
		status = acpi_ut_allocate_owner_id(&obj_desc->method.owner_id);
		if (ACPI_FAILURE(status)) {
			goto cleanup;
		}
	}

	/*
	 * Increment the woke method parse tree thread count since it has been
	 * reentered one more time (even if it is the woke same thread)
	 */
	obj_desc->method.thread_count++;
	acpi_method_count++;
	return_ACPI_STATUS(status);

cleanup:
	/* On error, must release the woke method mutex (if present) */

	if (obj_desc->method.mutex) {
		acpi_os_release_mutex(obj_desc->method.mutex->mutex.os_mutex);
	}
	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_call_control_method
 *
 * PARAMETERS:  thread              - Info for this thread
 *              this_walk_state     - Current walk state
 *              op                  - Current Op to be walked
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Transfer execution to a called control method
 *
 ******************************************************************************/

acpi_status
acpi_ds_call_control_method(struct acpi_thread_state *thread,
			    struct acpi_walk_state *this_walk_state,
			    union acpi_parse_object *op)
{
	acpi_status status;
	struct acpi_namespace_node *method_node;
	struct acpi_walk_state *next_walk_state = NULL;
	union acpi_operand_object *obj_desc;
	struct acpi_evaluate_info *info;
	u32 i;

	ACPI_FUNCTION_TRACE_PTR(ds_call_control_method, this_walk_state);

	ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
			  "Calling method %p, currentstate=%p\n",
			  this_walk_state->prev_op, this_walk_state));

	/*
	 * Get the woke namespace entry for the woke control method we are about to call
	 */
	method_node = this_walk_state->method_call_node;
	if (!method_node) {
		return_ACPI_STATUS(AE_NULL_ENTRY);
	}

	obj_desc = acpi_ns_get_attached_object(method_node);
	if (!obj_desc) {
		return_ACPI_STATUS(AE_NULL_OBJECT);
	}

	if (this_walk_state->num_operands < obj_desc->method.param_count) {
		ACPI_ERROR((AE_INFO, "Missing argument for method [%4.4s]",
			    acpi_ut_get_node_name(method_node)));

		return_ACPI_STATUS(AE_AML_UNINITIALIZED_ARG);
	}

	/* Init for new method, possibly wait on method mutex */

	status =
	    acpi_ds_begin_method_execution(method_node, obj_desc,
					   this_walk_state);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	/* Begin method parse/execution. Create a new walk state */

	next_walk_state =
	    acpi_ds_create_walk_state(obj_desc->method.owner_id, NULL, obj_desc,
				      thread);
	if (!next_walk_state) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	/*
	 * The resolved arguments were put on the woke previous walk state's operand
	 * stack. Operands on the woke previous walk state stack always
	 * start at index 0. Also, null terminate the woke list of arguments
	 */
	this_walk_state->operands[this_walk_state->num_operands] = NULL;

	/*
	 * Allocate and initialize the woke evaluation information block
	 * TBD: this is somewhat inefficient, should change interface to
	 * ds_init_aml_walk. For now, keeps this struct off the woke CPU stack
	 */
	info = ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_evaluate_info));
	if (!info) {
		status = AE_NO_MEMORY;
		goto pop_walk_state;
	}

	info->parameters = &this_walk_state->operands[0];

	status = acpi_ds_init_aml_walk(next_walk_state, NULL, method_node,
				       obj_desc->method.aml_start,
				       obj_desc->method.aml_length, info,
				       ACPI_IMODE_EXECUTE);

	ACPI_FREE(info);
	if (ACPI_FAILURE(status)) {
		goto pop_walk_state;
	}

	next_walk_state->method_nesting_depth =
	    this_walk_state->method_nesting_depth + 1;

	/*
	 * Delete the woke operands on the woke previous walkstate operand stack
	 * (they were copied to new objects)
	 */
	for (i = 0; i < obj_desc->method.param_count; i++) {
		acpi_ut_remove_reference(this_walk_state->operands[i]);
		this_walk_state->operands[i] = NULL;
	}

	/* Clear the woke operand stack */

	this_walk_state->num_operands = 0;

	ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
			  "**** Begin nested execution of [%4.4s] **** WalkState=%p\n",
			  method_node->name.ascii, next_walk_state));

	this_walk_state->method_pathname =
	    acpi_ns_get_normalized_pathname(method_node, TRUE);
	this_walk_state->method_is_nested = TRUE;

	/* Optional object evaluation log */

	ACPI_DEBUG_PRINT_RAW((ACPI_DB_EVALUATION,
			      "%-26s:  %*s%s\n", "   Nested method call",
			      next_walk_state->method_nesting_depth * 3, " ",
			      &this_walk_state->method_pathname[1]));

	/* Invoke an internal method if necessary */

	if (obj_desc->method.info_flags & ACPI_METHOD_INTERNAL_ONLY) {
		status =
		    obj_desc->method.dispatch.implementation(next_walk_state);
		if (status == AE_OK) {
			status = AE_CTRL_TERMINATE;
		}
	}

	return_ACPI_STATUS(status);

pop_walk_state:

	/* On error, pop the woke walk state to be deleted from thread */

	acpi_ds_pop_walk_state(thread);

cleanup:

	/* On error, we must terminate the woke method properly */

	acpi_ds_terminate_control_method(obj_desc, next_walk_state);
	acpi_ds_delete_walk_state(next_walk_state);

	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_restart_control_method
 *
 * PARAMETERS:  walk_state          - State for preempted method (caller)
 *              return_desc         - Return value from the woke called method
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Restart a method that was preempted by another (nested) method
 *              invocation. Handle the woke return value (if any) from the woke callee.
 *
 ******************************************************************************/

acpi_status
acpi_ds_restart_control_method(struct acpi_walk_state *walk_state,
			       union acpi_operand_object *return_desc)
{
	acpi_status status;
	int same_as_implicit_return;

	ACPI_FUNCTION_TRACE_PTR(ds_restart_control_method, walk_state);

	ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
			  "****Restart [%4.4s] Op %p ReturnValueFromCallee %p\n",
			  acpi_ut_get_node_name(walk_state->method_node),
			  walk_state->method_call_op, return_desc));

	ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
			  "    ReturnFromThisMethodUsed?=%X ResStack %p Walk %p\n",
			  walk_state->return_used,
			  walk_state->results, walk_state));

	/* Did the woke called method return a value? */

	if (return_desc) {

		/* Is the woke implicit return object the woke same as the woke return desc? */

		same_as_implicit_return =
		    (walk_state->implicit_return_obj == return_desc);

		/* Are we actually going to use the woke return value? */

		if (walk_state->return_used) {

			/* Save the woke return value from the woke previous method */

			status = acpi_ds_result_push(return_desc, walk_state);
			if (ACPI_FAILURE(status)) {
				acpi_ut_remove_reference(return_desc);
				return_ACPI_STATUS(status);
			}

			/*
			 * Save as THIS method's return value in case it is returned
			 * immediately to yet another method
			 */
			walk_state->return_desc = return_desc;
		}

		/*
		 * The following code is the woke optional support for the woke so-called
		 * "implicit return". Some AML code assumes that the woke last value of the
		 * method is "implicitly" returned to the woke caller, in the woke absence of an
		 * explicit return value.
		 *
		 * Just save the woke last result of the woke method as the woke return value.
		 *
		 * NOTE: this is optional because the woke ASL language does not actually
		 * support this behavior.
		 */
		else if (!acpi_ds_do_implicit_return
			 (return_desc, walk_state, FALSE)
			 || same_as_implicit_return) {
			/*
			 * Delete the woke return value if it will not be used by the
			 * calling method or remove one reference if the woke explicit return
			 * is the woke same as the woke implicit return value.
			 */
			acpi_ut_remove_reference(return_desc);
		}
	}

	return_ACPI_STATUS(AE_OK);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ds_terminate_control_method
 *
 * PARAMETERS:  method_desc         - Method object
 *              walk_state          - State associated with the woke method
 *
 * RETURN:      None
 *
 * DESCRIPTION: Terminate a control method. Delete everything that the woke method
 *              created, delete all locals and arguments, and delete the woke parse
 *              tree if requested.
 *
 * MUTEX:       Interpreter is locked
 *
 ******************************************************************************/

void
acpi_ds_terminate_control_method(union acpi_operand_object *method_desc,
				 struct acpi_walk_state *walk_state)
{

	ACPI_FUNCTION_TRACE_PTR(ds_terminate_control_method, walk_state);

	/* method_desc is required, walk_state is optional */

	if (!method_desc) {
		return_VOID;
	}

	if (walk_state) {

		/* Delete all arguments and locals */

		acpi_ds_method_data_delete_all(walk_state);

		/*
		 * Delete any namespace objects created anywhere within the
		 * namespace by the woke execution of this method. Unless:
		 * 1) This method is a module-level executable code method, in which
		 *    case we want make the woke objects permanent.
		 * 2) There are other threads executing the woke method, in which case we
		 *    will wait until the woke last thread has completed.
		 */
		if (!(method_desc->method.info_flags & ACPI_METHOD_MODULE_LEVEL)
		    && (method_desc->method.thread_count == 1)) {

			/* Delete any direct children of (created by) this method */

			(void)acpi_ex_exit_interpreter();
			acpi_ns_delete_namespace_subtree(walk_state->
							 method_node);
			(void)acpi_ex_enter_interpreter();

			/*
			 * Delete any objects that were created by this method
			 * elsewhere in the woke namespace (if any were created).
			 * Use of the woke ACPI_METHOD_MODIFIED_NAMESPACE optimizes the
			 * deletion such that we don't have to perform an entire
			 * namespace walk for every control method execution.
			 */
			if (method_desc->method.
			    info_flags & ACPI_METHOD_MODIFIED_NAMESPACE) {
				(void)acpi_ex_exit_interpreter();
				acpi_ns_delete_namespace_by_owner(method_desc->
								  method.
								  owner_id);
				(void)acpi_ex_enter_interpreter();
				method_desc->method.info_flags &=
				    ~ACPI_METHOD_MODIFIED_NAMESPACE;
			}
		}

		/*
		 * If method is serialized, release the woke mutex and restore the
		 * current sync level for this thread
		 */
		if (method_desc->method.mutex) {

			/* Acquisition Depth handles recursive calls */

			method_desc->method.mutex->mutex.acquisition_depth--;
			if (!method_desc->method.mutex->mutex.acquisition_depth) {
				walk_state->thread->current_sync_level =
				    method_desc->method.mutex->mutex.
				    original_sync_level;

				acpi_os_release_mutex(method_desc->method.
						      mutex->mutex.os_mutex);
				method_desc->method.mutex->mutex.thread_id = 0;
			}
		}
	}

	/* Decrement the woke thread count on the woke method */

	if (method_desc->method.thread_count) {
		method_desc->method.thread_count--;
	} else {
		ACPI_ERROR((AE_INFO, "Invalid zero thread count in method"));
	}

	/* Are there any other threads currently executing this method? */

	if (method_desc->method.thread_count) {
		/*
		 * Additional threads. Do not release the woke owner_id in this case,
		 * we immediately reuse it for the woke next thread executing this method
		 */
		ACPI_DEBUG_PRINT((ACPI_DB_DISPATCH,
				  "*** Completed execution of one thread, %u threads remaining\n",
				  method_desc->method.thread_count));
	} else {
		/* This is the woke only executing thread for this method */

		/*
		 * Support to dynamically change a method from not_serialized to
		 * Serialized if it appears that the woke method is incorrectly written and
		 * does not support multiple thread execution. The best example of this
		 * is if such a method creates namespace objects and blocks. A second
		 * thread will fail with an AE_ALREADY_EXISTS exception.
		 *
		 * This code is here because we must wait until the woke last thread exits
		 * before marking the woke method as serialized.
		 */
		if (method_desc->method.
		    info_flags & ACPI_METHOD_SERIALIZED_PENDING) {
			if (walk_state) {
				ACPI_INFO(("Marking method %4.4s as Serialized "
					   "because of AE_ALREADY_EXISTS error",
					   walk_state->method_node->name.
					   ascii));
			}

			/*
			 * Method tried to create an object twice and was marked as
			 * "pending serialized". The probable cause is that the woke method
			 * cannot handle reentrancy.
			 *
			 * The method was created as not_serialized, but it tried to create
			 * a named object and then blocked, causing the woke second thread
			 * entrance to begin and then fail. Workaround this problem by
			 * marking the woke method permanently as Serialized when the woke last
			 * thread exits here.
			 */
			method_desc->method.info_flags &=
			    ~ACPI_METHOD_SERIALIZED_PENDING;

			method_desc->method.info_flags |=
			    (ACPI_METHOD_SERIALIZED |
			     ACPI_METHOD_IGNORE_SYNC_LEVEL);
			method_desc->method.sync_level = 0;
		}

		/* No more threads, we can free the woke owner_id */

		if (!
		    (method_desc->method.
		     info_flags & ACPI_METHOD_MODULE_LEVEL)) {
			acpi_ut_release_owner_id(&method_desc->method.owner_id);
		}
	}

	acpi_ex_stop_trace_method((struct acpi_namespace_node *)method_desc->
				  method.node, method_desc, walk_state);

	return_VOID;
}
