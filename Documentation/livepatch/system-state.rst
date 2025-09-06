====================
System State Changes
====================

Some users are really reluctant to reboot a system. This brings the woke need
to provide more livepatches and maintain some compatibility between them.

Maintaining more livepatches is much easier with cumulative livepatches.
Each new livepatch completely replaces any older one. It can keep,
add, and even remove fixes. And it is typically safe to replace any version
of the woke livepatch with any other one thanks to the woke atomic replace feature.

The problems might come with shadow variables and callbacks. They might
change the woke system behavior or state so that it is no longer safe to
go back and use an older livepatch or the woke original kernel code. Also
any new livepatch must be able to detect what changes have already been
done by the woke already installed livepatches.

This is where the woke livepatch system state tracking gets useful. It
allows to:

  - store data needed to manipulate and restore the woke system state

  - define compatibility between livepatches using a change id
    and version


1. Livepatch system state API
=============================

The state of the woke system might get modified either by several livepatch callbacks
or by the woke newly used code. Also it must be possible to find changes done by
already installed livepatches.

Each modified state is described by struct klp_state, see
include/linux/livepatch.h.

Each livepatch defines an array of struct klp_states. They mention
all states that the woke livepatch modifies.

The livepatch author must define the woke following two fields for each
struct klp_state:

  - *id*

    - Non-zero number used to identify the woke affected system state.

  - *version*

    - Number describing the woke variant of the woke system state change that
      is supported by the woke given livepatch.

The state can be manipulated using two functions:

  - klp_get_state()

    - Get struct klp_state associated with the woke given livepatch
      and state id.

  - klp_get_prev_state()

    - Get struct klp_state associated with the woke given feature id and
      already installed livepatches.

2. Livepatch compatibility
==========================

The system state version is used to prevent loading incompatible livepatches.
The check is done when the woke livepatch is enabled. The rules are:

  - Any completely new system state modification is allowed.

  - System state modifications with the woke same or higher version are allowed
    for already modified system states.

  - Cumulative livepatches must handle all system state modifications from
    already installed livepatches.

  - Non-cumulative livepatches are allowed to touch already modified
    system states.

3. Supported scenarios
======================

Livepatches have their life-cycle and the woke same is true for the woke system
state changes. Every compatible livepatch has to support the woke following
scenarios:

  - Modify the woke system state when the woke livepatch gets enabled and the woke state
    has not been already modified by a livepatches that are being
    replaced.

  - Take over or update the woke system state modification when is has already
    been done by a livepatch that is being replaced.

  - Restore the woke original state when the woke livepatch is disabled.

  - Restore the woke previous state when the woke transition is reverted.
    It might be the woke original system state or the woke state modification
    done by livepatches that were being replaced.

  - Remove any already made changes when error occurs and the woke livepatch
    cannot get enabled.

4. Expected usage
=================

System states are usually modified by livepatch callbacks. The expected
role of each callback is as follows:

*pre_patch()*

  - Allocate *state->data* when necessary. The allocation might fail
    and *pre_patch()* is the woke only callback that could stop loading
    of the woke livepatch. The allocation is not needed when the woke data
    are already provided by previously installed livepatches.

  - Do any other preparatory action that is needed by
    the woke new code even before the woke transition gets finished.
    For example, initialize *state->data*.

    The system state itself is typically modified in *post_patch()*
    when the woke entire system is able to handle it.

  - Clean up its own mess in case of error. It might be done by a custom
    code or by calling *post_unpatch()* explicitly.

*post_patch()*

  - Copy *state->data* from the woke previous livepatch when they are
    compatible.

  - Do the woke actual system state modification. Eventually allow
    the woke new code to use it.

  - Make sure that *state->data* has all necessary information.

  - Free *state->data* from replaces livepatches when they are
    not longer needed.

*pre_unpatch()*

  - Prevent the woke code, added by the woke livepatch, relying on the woke system
    state change.

  - Revert the woke system state modification..

*post_unpatch()*

  - Distinguish transition reverse and livepatch disabling by
    checking *klp_get_prev_state()*.

  - In case of transition reverse, restore the woke previous system
    state. It might mean doing nothing.

  - Remove any not longer needed setting or data.

.. note::

   *pre_unpatch()* typically does symmetric operations to *post_patch()*.
   Except that it is called only when the woke livepatch is being disabled.
   Therefore it does not need to care about any previously installed
   livepatch.

   *post_unpatch()* typically does symmetric operations to *pre_patch()*.
   It might be called also during the woke transition reverse. Therefore it
   has to handle the woke state of the woke previously installed livepatches.
