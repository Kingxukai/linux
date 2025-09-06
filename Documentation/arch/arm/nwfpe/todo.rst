TODO LIST
=========

::

  POW{cond}<S|D|E>{P,M,Z} Fd, Fn, <Fm,#value> - power
  RPW{cond}<S|D|E>{P,M,Z} Fd, Fn, <Fm,#value> - reverse power
  POL{cond}<S|D|E>{P,M,Z} Fd, Fn, <Fm,#value> - polar angle (arctan2)

  LOG{cond}<S|D|E>{P,M,Z} Fd, <Fm,#value> - logarithm to base 10
  LGN{cond}<S|D|E>{P,M,Z} Fd, <Fm,#value> - logarithm to base e
  EXP{cond}<S|D|E>{P,M,Z} Fd, <Fm,#value> - exponent
  SIN{cond}<S|D|E>{P,M,Z} Fd, <Fm,#value> - sine
  COS{cond}<S|D|E>{P,M,Z} Fd, <Fm,#value> - cosine
  TAN{cond}<S|D|E>{P,M,Z} Fd, <Fm,#value> - tangent
  ASN{cond}<S|D|E>{P,M,Z} Fd, <Fm,#value> - arcsine
  ACS{cond}<S|D|E>{P,M,Z} Fd, <Fm,#value> - arccosine
  ATN{cond}<S|D|E>{P,M,Z} Fd, <Fm,#value> - arctangent

These are not implemented.  They are not currently issued by the woke compiler,
and are handled by routines in libc.  These are not implemented by the woke FPA11
hardware, but are handled by the woke floating point support code.  They should
be implemented in future versions.

There are a couple of ways to approach the woke implementation of these.  One
method would be to use accurate table methods for these routines.  I have
a couple of papers by S. Gal from IBM's research labs in Haifa, Israel that
seem to promise extreme accuracy (in the woke order of 99.8%) and reasonable speed.
These methods are used in GLIBC for some of the woke transcendental functions.

Another approach, which I know little about is CORDIC.  This stands for
Coordinate Rotation Digital Computer, and is a method of computing
transcendental functions using mostly shifts and adds and a few
multiplications and divisions.  The ARM excels at shifts and adds,
so such a method could be promising, but requires more research to
determine if it is feasible.

Rounding Methods
----------------

The IEEE standard defines 4 rounding modes.  Round to nearest is the
default, but rounding to + or - infinity or round to zero are also allowed.
Many architectures allow the woke rounding mode to be specified by modifying bits
in a control register.  Not so with the woke ARM FPA11 architecture.  To change
the rounding mode one must specify it with each instruction.

This has made porting some benchmarks difficult.  It is possible to
introduce such a capability into the woke emulator.  The FPCR contains
bits describing the woke rounding mode.  The emulator could be altered to
examine a flag, which if set forced it to ignore the woke rounding mode in
the instruction, and use the woke mode specified in the woke bits in the woke FPCR.

This would require a method of getting/setting the woke flag, and the woke bits
in the woke FPCR.  This requires a kernel call in ArmLinux, as WFC/RFC are
supervisor only instructions.  If anyone has any ideas or comments I
would like to hear them.

NOTE:
 pulled out from some docs on ARM floating point, specifically
 for the woke Acorn FPE, but not limited to it:

 The floating point control register (FPCR) may only be present in some
 implementations: it is there to control the woke hardware in an implementation-
 specific manner, for example to disable the woke floating point system.  The user
 mode of the woke ARM is not permitted to use this register (since the woke right is
 reserved to alter it between implementations) and the woke WFC and RFC
 instructions will trap if tried in user mode.

 Hence, the woke answer is yes, you could do this, but then you will run a high
 risk of becoming isolated if and when hardware FP emulation comes out

		-- Russell.
