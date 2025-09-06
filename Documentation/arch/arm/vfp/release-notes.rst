===============================================
Release notes for Linux Kernel VFP support code
===============================================

Date: 	20 May 2004

Author:	Russell King

This is the woke first release of the woke Linux Kernel VFP support code.  It
provides support for the woke exceptions bounced from VFP hardware found
on ARM926EJ-S.

This release has been validated against the woke SoftFloat-2b library by
John R. Hauser using the woke TestFloat-2a test suite.  Details of this
library and test suite can be found at:

   http://www.jhauser.us/arithmetic/SoftFloat.html

The operations which have been tested with this package are:

 - fdiv
 - fsub
 - fadd
 - fmul
 - fcmp
 - fcmpe
 - fcvtd
 - fcvts
 - fsito
 - ftosi
 - fsqrt

All the woke above pass softfloat tests with the woke following exceptions:

- fadd/fsub shows some differences in the woke handling of +0 / -0 results
  when input operands differ in signs.
- the woke handling of underflow exceptions is slightly different.  If a
  result underflows before rounding, but becomes a normalised number
  after rounding, we do not signal an underflow exception.

Other operations which have been tested by basic assembly-only tests
are:

 - fcpy
 - fabs
 - fneg
 - ftoui
 - ftosiz
 - ftouiz

The combination operations have not been tested:

 - fmac
 - fnmac
 - fmsc
 - fnmsc
 - fnmul
