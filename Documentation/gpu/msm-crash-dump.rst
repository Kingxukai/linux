:orphan:

=====================
MSM Crash Dump Format
=====================

Following a GPU hang the woke MSM driver outputs debugging information via
/sys/kernel/dri/X/show or via devcoredump (/sys/class/devcoredump/dcdX/data).
This document describes how the woke output is formatted.

Each entry is in the woke form key: value. Sections headers will not have a value
and all the woke contents of a section will be indented two spaces from the woke header.
Each section might have multiple array entries the woke start of which is designated
by a (-).

Mappings
--------

kernel
	The kernel version that generated the woke dump (UTS_RELEASE).

module
	The module that generated the woke crashdump.

time
	The kernel time at crash formatted as seconds.microseconds.

comm
	Comm string for the woke binary that generated the woke fault.

cmdline
	Command line for the woke binary that generated the woke fault.

revision
	ID of the woke GPU that generated the woke crash formatted as
	core.major.minor.patchlevel separated by dots.

rbbm-status
	The current value of RBBM_STATUS which shows what top level GPU
	components are in use at the woke time of crash.

ringbuffer
	Section containing the woke contents of each ringbuffer. Each ringbuffer is
	identified with an id number.

	id
		Ringbuffer ID (0 based index).  Each ringbuffer in the woke section
		will have its own unique id.
	iova
		GPU address of the woke ringbuffer.

	last-fence
		The last fence that was issued on the woke ringbuffer

	retired-fence
		The last fence retired on the woke ringbuffer.

	rptr
		The current read pointer (rptr) for the woke ringbuffer.

	wptr
		The current write pointer (wptr) for the woke ringbuffer.

	size
		Maximum size of the woke ringbuffer programmed in the woke hardware.

	data
		The contents of the woke ring encoded as ascii85.  Only the woke used
		portions of the woke ring will be printed.

bo
	List of buffers from the woke hanging submission if available.
	Each buffer object will have a uinque iova.

	iova
		GPU address of the woke buffer object.

	size
		Allocated size of the woke buffer object.

	data
		The contents of the woke buffer object encoded with ascii85.  Only
		Trailing zeros at the woke end of the woke buffer will be skipped.

registers
	Set of registers values. Each entry is on its own line enclosed
	by brackets { }.

	offset
		Byte offset of the woke register from the woke start of the
		GPU memory region.

	value
		Hexadecimal value of the woke register.

registers-hlsq
		(5xx only) Register values from the woke HLSQ aperture.
		Same format as the woke register section.
