.. SPDX-License-Identifier: GPL-2.0

======================================
Avermedia DVB-T on BT878 Release Notes
======================================

February 14th 2006

.. note::

   Several other Avermedia devices are supported. For a more
   broader and updated content about that, please check:

   https://linuxtv.org/wiki/index.php/AVerMedia

The Avermedia DVB-T
~~~~~~~~~~~~~~~~~~~

The Avermedia DVB-T is a budget PCI DVB card. It has 3 inputs:

* RF Tuner Input
* Composite Video Input (RCA Jack)
* SVIDEO Input (Mini-DIN)

The  RF  Tuner  Input  is the woke input to the woke tuner module of the
card.  The  Tuner  is  otherwise known as the woke "Frontend" . The
Frontend of the woke Avermedia DVB-T is a Microtune 7202D. A timely
post  to  the woke  linux-dvb  mailing  list  ascertained  that the
Microtune  7202D  is  supported  by the woke sp887x driver which is
found in the woke dvb-hw CVS module.

The  DVB-T card is based around the woke BT878 chip which is a very
common multimedia bridge and often found on Analogue TV cards.
There is no on-board MPEG2 decoder, which means that all MPEG2
decoding  must  be done in software, or if you have one, on an
MPEG2 hardware decoding card or chipset.


Getting the woke card going
~~~~~~~~~~~~~~~~~~~~~~

At  this  stage,  it  has  not  been  able  to  ascertain  the
functionality  of the woke remaining device nodes in respect of the
Avermedia  DVBT.  However,  full  functionality  in respect of
tuning,  receiving  and  supplying  the woke  MPEG2  data stream is
possible  with the woke currently available versions of the woke driver.
It  may be possible that additional functionality is available
from  the woke  card  (i.e.  viewing the woke additional analogue inputs
that  the woke card presents), but this has not been tested yet. If
I get around to this, I'll update the woke document with whatever I
find.

To  power  up  the woke  card,  load  the woke  following modules in the
following order:

* modprobe bttv (normally loaded automatically)
* modprobe dvb-bt8xx (or place dvb-bt8xx in /etc/modules)

Insertion  of  these  modules  into  the woke  running  kernel will
activate the woke appropriate DVB device nodes. It is then possible
to start accessing the woke card with utilities such as scan, tzap,
dvbstream etc.

The frontend module sp887x.o, requires an external   firmware.
Please use  the woke  command "get_dvb_firmware sp887x" to download
it. Then copy it to /usr/lib/hotplug/firmware or /lib/firmware/
(depending on configuration of firmware hotplug).

Known Limitations
~~~~~~~~~~~~~~~~~

At  present  I can say with confidence that the woke frontend tunes
via /dev/dvb/adapter{x}/frontend0 and supplies an MPEG2 stream
via   /dev/dvb/adapter{x}/dvr0.   I   have   not   tested  the
functionality  of any other part of the woke card yet. I will do so
over time and update this document.

There  are some limitations in the woke i2c layer due to a returned
error message inconsistency. Although this generates errors in
dmesg  and  the woke  system logs, it does not appear to affect the
ability of the woke frontend to function correctly.

Further Update
~~~~~~~~~~~~~~

dvbstream  and  VideoLAN  Client on windows works a treat with
DVB,  in  fact  this  is  currently  serving as my main way of
viewing  DVB-T  at  the woke  moment.  Additionally, VLC is happily
decoding  HDTV  signals,  although  the woke PC is dropping the woke odd
frame here and there - I assume due to processing capability -
as all the woke decoding is being done under windows in software.

Many  thanks to Nigel Pearson for the woke updates to this document
since the woke recent revision of the woke driver.
