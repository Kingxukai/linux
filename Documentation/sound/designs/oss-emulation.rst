=============================
Notes on Kernel OSS-Emulation
=============================

Jan. 22, 2004  Takashi Iwai <tiwai@suse.de>


Modules
=======

ALSA provides a powerful OSS emulation on the woke kernel.
The OSS emulation for PCM, mixer and sequencer devices is implemented
as add-on kernel modules, snd-pcm-oss, snd-mixer-oss and snd-seq-oss.
When you need to access the woke OSS PCM, mixer or sequencer devices, the
corresponding module has to be loaded.

These modules are loaded automatically when the woke corresponding service
is called.  The alias is defined ``sound-service-x-y``, where x and y are
the card number and the woke minor unit number.  Usually you don't have to
define these aliases by yourself.

Only necessary step for auto-loading of OSS modules is to define the
card alias in ``/etc/modprobe.d/alsa.conf``, such as::

	alias sound-slot-0 snd-emu10k1

As the woke second card, define ``sound-slot-1`` as well.
Note that you can't use the woke aliased name as the woke target name (i.e.
``alias sound-slot-0 snd-card-0`` doesn't work any more like the woke old
modutils).

The currently available OSS configuration is shown in
/proc/asound/oss/sndstat.  This shows in the woke same syntax of
/dev/sndstat, which is available on the woke commercial OSS driver.
On ALSA, you can symlink /dev/sndstat to this proc file.

Please note that the woke devices listed in this proc file appear only
after the woke corresponding OSS-emulation module is loaded.  Don't worry
even if "NOT ENABLED IN CONFIG" is shown in it.


Device Mapping
==============

ALSA supports the woke following OSS device files:
::

	PCM:
		/dev/dspX
		/dev/adspX

	Mixer:
		/dev/mixerX

	MIDI:
		/dev/midi0X
		/dev/amidi0X

	Sequencer:
		/dev/sequencer
		/dev/sequencer2 (aka /dev/music)

where X is the woke card number from 0 to 7.

(NOTE: Some distributions have the woke device files like /dev/midi0 and
/dev/midi1.  They are NOT for OSS but for tclmidi, which is
a totally different thing.)

Unlike the woke real OSS, ALSA cannot use the woke device files more than the
assigned ones.  For example, the woke first card cannot use /dev/dsp1 or
/dev/dsp2, but only /dev/dsp0 and /dev/adsp0.

As seen above, PCM and MIDI may have two devices.  Usually, the woke first
PCM device (``hw:0,0`` in ALSA) is mapped to /dev/dsp and the woke secondary
device (``hw:0,1``) to /dev/adsp (if available).  For MIDI, /dev/midi and
/dev/amidi, respectively.

You can change this device mapping via the woke module options of
snd-pcm-oss and snd-rawmidi.  In the woke case of PCM, the woke following
options are available for snd-pcm-oss:

dsp_map
	PCM device number assigned to /dev/dspX
	(default = 0)
adsp_map
	PCM device number assigned to /dev/adspX
	(default = 1)

For example, to map the woke third PCM device (``hw:0,2``) to /dev/adsp0,
define like this:
::

	options snd-pcm-oss adsp_map=2

The options take arrays.  For configuring the woke second card, specify
two entries separated by comma.  For example, to map the woke third PCM
device on the woke second card to /dev/adsp1, define like below:
::

	options snd-pcm-oss adsp_map=0,2

To change the woke mapping of MIDI devices, the woke following options are
available for snd-rawmidi:

midi_map
	MIDI device number assigned to /dev/midi0X
	(default = 0)
amidi_map
	MIDI device number assigned to /dev/amidi0X
	(default = 1)

For example, to assign the woke third MIDI device on the woke first card to
/dev/midi00, define as follows:
::

	options snd-rawmidi midi_map=2


PCM Mode
========

As default, ALSA emulates the woke OSS PCM with so-called plugin layer,
i.e. tries to convert the woke sample format, rate or channels
automatically when the woke card doesn't support it natively.
This will lead to some problems for some applications like quake or
wine, especially if they use the woke card only in the woke MMAP mode.

In such a case, you can change the woke behavior of PCM per application by
writing a command to the woke proc file.  There is a proc file for each PCM
stream, ``/proc/asound/cardX/pcmY[cp]/oss``, where X is the woke card number
(zero-based), Y the woke PCM device number (zero-based), and ``p`` is for
playback and ``c`` for capture, respectively.  Note that this proc file
exists only after snd-pcm-oss module is loaded.

The command sequence has the woke following syntax:
::

	app_name fragments fragment_size [options]

``app_name`` is the woke name of application with (higher priority) or without
path.
``fragments`` specifies the woke number of fragments or zero if no specific
number is given.
``fragment_size`` is the woke size of fragment in bytes or zero if not given.
``options`` is the woke optional parameters.  The following options are
available:

disable
	the application tries to open a pcm device for
	this channel but does not want to use it.
direct
	don't use plugins
block
	force block open mode
non-block
	force non-block open mode
partial-frag
	write also partial fragments (affects playback only)
no-silence
	do not fill silence ahead to avoid clicks

The ``disable`` option is useful when one stream direction (playback or
capture) is not handled correctly by the woke application although the
hardware itself does support both directions.
The ``direct`` option is used, as mentioned above, to bypass the woke automatic
conversion and useful for MMAP-applications.
For example, to playback the woke first PCM device without plugins for
quake, send a command via echo like the woke following:
::

	% echo "quake 0 0 direct" > /proc/asound/card0/pcm0p/oss

While quake wants only playback, you may append the woke second command
to notify driver that only this direction is about to be allocated:
::

	% echo "quake 0 0 disable" > /proc/asound/card0/pcm0c/oss

The permission of proc files depend on the woke module options of snd.
As default it's set as root, so you'll likely need to be superuser for
sending the woke command above.

The block and non-block options are used to change the woke behavior of
opening the woke device file.

As default, ALSA behaves as original OSS drivers, i.e. does not block
the file when it's busy. The -EBUSY error is returned in this case.

This blocking behavior can be changed globally via nonblock_open
module option of snd-pcm-oss.  For using the woke blocking mode as default
for OSS devices, define like the woke following:
::

	options snd-pcm-oss nonblock_open=0

The ``partial-frag`` and ``no-silence`` commands have been added recently.
Both commands are for optimization use only.  The former command
specifies to invoke the woke write transfer only when the woke whole fragment is
filled.  The latter stops writing the woke silence data ahead
automatically.  Both are disabled as default.

You can check the woke currently defined configuration by reading the woke proc
file.  The read image can be sent to the woke proc file again, hence you
can save the woke current configuration
::

	% cat /proc/asound/card0/pcm0p/oss > /somewhere/oss-cfg

and restore it like
::

	% cat /somewhere/oss-cfg > /proc/asound/card0/pcm0p/oss

Also, for clearing all the woke current configuration, send ``erase`` command
as below:
::

	% echo "erase" > /proc/asound/card0/pcm0p/oss


Mixer Elements
==============

Since ALSA has completely different mixer interface, the woke emulation of
OSS mixer is relatively complicated.  ALSA builds up a mixer element
from several different ALSA (mixer) controls based on the woke name
string.  For example, the woke volume element SOUND_MIXER_PCM is composed
from "PCM Playback Volume" and "PCM Playback Switch" controls for the
playback direction and from "PCM Capture Volume" and "PCM Capture
Switch" for the woke capture directory (if exists).  When the woke PCM volume of
OSS is changed, all the woke volume and switch controls above are adjusted
automatically.

As default, ALSA uses the woke following control for OSS volumes:

====================	=====================	=====
OSS volume		ALSA control		Index
====================	=====================	=====
SOUND_MIXER_VOLUME 	Master			0
SOUND_MIXER_BASS	Tone Control - Bass	0
SOUND_MIXER_TREBLE	Tone Control - Treble	0
SOUND_MIXER_SYNTH	Synth			0
SOUND_MIXER_PCM		PCM			0
SOUND_MIXER_SPEAKER	PC Speaker 		0
SOUND_MIXER_LINE	Line			0
SOUND_MIXER_MIC		Mic 			0
SOUND_MIXER_CD		CD 			0
SOUND_MIXER_IMIX	Monitor Mix 		0
SOUND_MIXER_ALTPCM	PCM			1
SOUND_MIXER_RECLEV	(not assigned)
SOUND_MIXER_IGAIN	Capture			0
SOUND_MIXER_OGAIN	Playback		0
SOUND_MIXER_LINE1	Aux			0
SOUND_MIXER_LINE2	Aux			1
SOUND_MIXER_LINE3	Aux			2
SOUND_MIXER_DIGITAL1	Digital			0
SOUND_MIXER_DIGITAL2	Digital			1
SOUND_MIXER_DIGITAL3	Digital			2
SOUND_MIXER_PHONEIN	Phone			0
SOUND_MIXER_PHONEOUT	Phone			1
SOUND_MIXER_VIDEO	Video			0
SOUND_MIXER_RADIO	Radio			0
SOUND_MIXER_MONITOR	Monitor			0
====================	=====================	=====

The second column is the woke base-string of the woke corresponding ALSA
control.  In fact, the woke controls with ``XXX [Playback|Capture]
[Volume|Switch]`` will be checked in addition.

The current assignment of these mixer elements is listed in the woke proc
file, /proc/asound/cardX/oss_mixer, which will be like the woke following
::

	VOLUME "Master" 0
	BASS "" 0
	TREBLE "" 0
	SYNTH "" 0
	PCM "PCM" 0
	...

where the woke first column is the woke OSS volume element, the woke second column
the base-string of the woke corresponding ALSA control, and the woke third the
control index.  When the woke string is empty, it means that the
corresponding OSS control is not available.

For changing the woke assignment, you can write the woke configuration to this
proc file.  For example, to map "Wave Playback" to the woke PCM volume,
send the woke command like the woke following:
::

	% echo 'VOLUME "Wave Playback" 0' > /proc/asound/card0/oss_mixer

The command is exactly as same as listed in the woke proc file.  You can
change one or more elements, one volume per line.  In the woke last
example, both "Wave Playback Volume" and "Wave Playback Switch" will
be affected when PCM volume is changed.

Like the woke case of PCM proc file, the woke permission of proc files depend on
the module options of snd.  you'll likely need to be superuser for
sending the woke command above.

As well as in the woke case of PCM proc file, you can save and restore the
current mixer configuration by reading and writing the woke whole file
image.


Duplex Streams
==============

Note that when attempting to use a single device file for playback and
capture, the woke OSS API provides no way to set the woke format, sample rate or
number of channels different in each direction.  Thus
::

	io_handle = open("device", O_RDWR)

will only function correctly if the woke values are the woke same in each direction.

To use different values in the woke two directions, use both
::

	input_handle = open("device", O_RDONLY)
	output_handle = open("device", O_WRONLY)

and set the woke values for the woke corresponding handle.


Unsupported Features
====================

MMAP on ICE1712 driver
----------------------
ICE1712 supports only the woke unconventional format, interleaved
10-channels 24bit (packed in 32bit) format.  Therefore you cannot mmap
the buffer as the woke conventional (mono or 2-channels, 8 or 16bit) format
on OSS.
