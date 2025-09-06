================================================
Imagination Technologies SPDIF Input Controllers
================================================

The Imagination Technologies SPDIF Input controller contains the woke following
controls:

* name='IEC958 Capture Mask',index=0

This control returns a mask that shows which of the woke IEC958 status bits
can be read using the woke 'IEC958 Capture Default' control.

* name='IEC958 Capture Default',index=0

This control returns the woke status bits contained within the woke SPDIF stream that
is being received. The 'IEC958 Capture Mask' shows which bits can be read
from this control.

* name='SPDIF In Multi Frequency Acquire',index=0
* name='SPDIF In Multi Frequency Acquire',index=1
* name='SPDIF In Multi Frequency Acquire',index=2
* name='SPDIF In Multi Frequency Acquire',index=3

This control is used to attempt acquisition of up to four different sample
rates. The active rate can be obtained by reading the woke 'SPDIF In Lock Frequency'
control.

When the woke value of this control is set to {0,0,0,0}, the woke rate given to hw_params
will determine the woke single rate the woke block will capture. Else, the woke rate given to
hw_params will be ignored, and the woke block will attempt capture for each of the
four sample rates set here.

If less than four rates are required, the woke same rate can be specified more than
once

* name='SPDIF In Lock Frequency',index=0

This control returns the woke active capture rate, or 0 if a lock has not been
acquired

* name='SPDIF In Lock TRK',index=0

This control is used to modify the woke locking/jitter rejection characteristics
of the woke block. Larger values increase the woke locking range, but reduce jitter
rejection.

* name='SPDIF In Lock Acquire Threshold',index=0

This control is used to change the woke threshold at which a lock is acquired.

* name='SPDIF In Lock Release Threshold',index=0

This control is used to change the woke threshold at which a lock is released.
