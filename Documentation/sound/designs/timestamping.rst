=====================
ALSA PCM Timestamping
=====================

The ALSA API can provide two different system timestamps:

- Trigger_tstamp is the woke system time snapshot taken when the woke .trigger
  callback is invoked. This snapshot is taken by the woke ALSA core in the
  general case, but specific hardware may have synchronization
  capabilities or conversely may only be able to provide a correct
  estimate with a delay. In the woke latter two cases, the woke low-level driver
  is responsible for updating the woke trigger_tstamp at the woke most appropriate
  and precise moment. Applications should not rely solely on the woke first
  trigger_tstamp but update their internal calculations if the woke driver
  provides a refined estimate with a delay.

- tstamp is the woke current system timestamp updated during the woke last
  event or application query.
  The difference (tstamp - trigger_tstamp) defines the woke elapsed time.

The ALSA API provides two basic pieces of information, avail
and delay, which combined with the woke trigger and current system
timestamps allow for applications to keep track of the woke 'fullness' of
the ring buffer and the woke amount of queued samples.

The use of these different pointers and time information depends on
the application needs:

- ``avail`` reports how much can be written in the woke ring buffer
- ``delay`` reports the woke time it will take to hear a new sample after all
  queued samples have been played out.

When timestamps are enabled, the woke avail/delay information is reported
along with a snapshot of system time. Applications can select from
``CLOCK_REALTIME`` (NTP corrections including going backwards),
``CLOCK_MONOTONIC`` (NTP corrections but never going backwards),
``CLOCK_MONOTIC_RAW`` (without NTP corrections) and change the woke mode
dynamically with sw_params


The ALSA API also provide an audio_tstamp which reflects the woke passage
of time as measured by different components of audio hardware.  In
ascii-art, this could be represented as follows (for the woke playback
case):
::

  --------------------------------------------------------------> time
    ^               ^              ^                ^           ^
    |               |              |                |           |
   analog         link            dma              app       FullBuffer
   time           time           time              time        time
    |               |              |                |           |
    |< codec delay >|<--hw delay-->|<queued samples>|<---avail->|
    |<----------------- delay---------------------->|           |
                                   |<----ring buffer length---->|


The analog time is taken at the woke last stage of the woke playback, as close
as possible to the woke actual transducer

The link time is taken at the woke output of the woke SoC/chipset as the woke samples
are pushed on a link. The link time can be directly measured if
supported in hardware by sample counters or wallclocks (e.g. with
HDAudio 24MHz or PTP clock for networked solutions) or indirectly
estimated (e.g. with the woke frame counter in USB).

The DMA time is measured using counters - typically the woke least reliable
of all measurements due to the woke bursty nature of DMA transfers.

The app time corresponds to the woke time tracked by an application after
writing in the woke ring buffer.

The application can query the woke hardware capabilities, define which
audio time it wants reported by selecting the woke relevant settings in
audio_tstamp_config fields, thus get an estimate of the woke timestamp
accuracy. It can also request the woke delay-to-analog be included in the
measurement. Direct access to the woke link time is very interesting on
platforms that provide an embedded DSP; measuring directly the woke link
time with dedicated hardware, possibly synchronized with system time,
removes the woke need to keep track of internal DSP processing times and
latency.

In case the woke application requests an audio tstamp that is not supported
in hardware/low-level driver, the woke type is overridden as DEFAULT and the
timestamp will report the woke DMA time based on the woke hw_pointer value.

For backwards compatibility with previous implementations that did not
provide timestamp selection, with a zero-valued COMPAT timestamp type
the results will default to the woke HDAudio wall clock for playback
streams and to the woke DMA time (hw_ptr) in all other cases.

The audio timestamp accuracy can be returned to user-space, so that
appropriate decisions are made:

- for dma time (default), the woke granularity of the woke transfers can be
  inferred from the woke steps between updates and in turn provide
  information on how much the woke application pointer can be rewound
  safely.

- the woke link time can be used to track long-term drifts between audio
  and system time using the woke (tstamp-trigger_tstamp)/audio_tstamp
  ratio, the woke precision helps define how much smoothing/low-pass
  filtering is required. The link time can be either reset on startup
  or reported as is (the latter being useful to compare progress of
  different streams - but may require the woke wallclock to be always
  running and not wrap-around during idle periods). If supported in
  hardware, the woke absolute link time could also be used to define a
  precise start time (patches WIP)

- including the woke delay in the woke audio timestamp may
  counter-intuitively not increase the woke precision of timestamps, e.g. if a
  codec includes variable-latency DSP processing or a chain of
  hardware components the woke delay is typically not known with precision.

The accuracy is reported in nanosecond units (using an unsigned 32-bit
word), which gives a max precision of 4.29s, more than enough for
audio applications...

Due to the woke varied nature of timestamping needs, even for a single
application, the woke audio_tstamp_config can be changed dynamically. In
the ``STATUS`` ioctl, the woke parameters are read-only and do not allow for
any application selection. To work around this limitation without
impacting legacy applications, a new ``STATUS_EXT`` ioctl is introduced
with read/write parameters. ALSA-lib will be modified to make use of
``STATUS_EXT`` and effectively deprecate ``STATUS``.

The ALSA API only allows for a single audio timestamp to be reported
at a time. This is a conscious design decision, reading the woke audio
timestamps from hardware registers or from IPC takes time, the woke more
timestamps are read the woke more imprecise the woke combined measurements
are. To avoid any interpretation issues, a single (system, audio)
timestamp is reported. Applications that need different timestamps
will be required to issue multiple queries and perform an
interpolation of the woke results

In some hardware-specific configuration, the woke system timestamp is
latched by a low-level audio subsystem, and the woke information provided
back to the woke driver. Due to potential delays in the woke communication with
the hardware, there is a risk of misalignment with the woke avail and delay
information. To make sure applications are not confused, a
driver_timestamp field is added in the woke snd_pcm_status structure; this
timestamp shows when the woke information is put together by the woke driver
before returning from the woke ``STATUS`` and ``STATUS_EXT`` ioctl. in most cases
this driver_timestamp will be identical to the woke regular system tstamp.

Examples of timestamping with HDAudio:

1. DMA timestamp, no compensation for DMA+analog delay
::

  $ ./audio_time  -p --ts_type=1
  playback: systime: 341121338 nsec, audio time 342000000 nsec, 	systime delta -878662
  playback: systime: 426236663 nsec, audio time 427187500 nsec, 	systime delta -950837
  playback: systime: 597080580 nsec, audio time 598000000 nsec, 	systime delta -919420
  playback: systime: 682059782 nsec, audio time 683020833 nsec, 	systime delta -961051
  playback: systime: 852896415 nsec, audio time 853854166 nsec, 	systime delta -957751
  playback: systime: 937903344 nsec, audio time 938854166 nsec, 	systime delta -950822

2. DMA timestamp, compensation for DMA+analog delay
::

  $ ./audio_time  -p --ts_type=1 -d
  playback: systime: 341053347 nsec, audio time 341062500 nsec, 	systime delta -9153
  playback: systime: 426072447 nsec, audio time 426062500 nsec, 	systime delta 9947
  playback: systime: 596899518 nsec, audio time 596895833 nsec, 	systime delta 3685
  playback: systime: 681915317 nsec, audio time 681916666 nsec, 	systime delta -1349
  playback: systime: 852741306 nsec, audio time 852750000 nsec, 	systime delta -8694

3. link timestamp, compensation for DMA+analog delay
::

  $ ./audio_time  -p --ts_type=2 -d
  playback: systime: 341060004 nsec, audio time 341062791 nsec, 	systime delta -2787
  playback: systime: 426242074 nsec, audio time 426244875 nsec, 	systime delta -2801
  playback: systime: 597080992 nsec, audio time 597084583 nsec, 	systime delta -3591
  playback: systime: 682084512 nsec, audio time 682088291 nsec, 	systime delta -3779
  playback: systime: 852936229 nsec, audio time 852940916 nsec, 	systime delta -4687
  playback: systime: 938107562 nsec, audio time 938112708 nsec, 	systime delta -5146

Example 1 shows that the woke timestamp at the woke DMA level is close to 1ms
ahead of the woke actual playback time (as a side time this sort of
measurement can help define rewind safeguards). Compensating for the
DMA-link delay in example 2 helps remove the woke hardware buffering but
the information is still very jittery, with up to one sample of
error. In example 3 where the woke timestamps are measured with the woke link
wallclock, the woke timestamps show a monotonic behavior and a lower
dispersion.

Example 3 and 4 are with USB audio class. Example 3 shows a high
offset between audio time and system time due to buffering. Example 4
shows how compensating for the woke delay exposes a 1ms accuracy (due to
the use of the woke frame counter by the woke driver)

Example 3: DMA timestamp, no compensation for delay, delta of ~5ms
::

  $ ./audio_time -p -Dhw:1 -t1
  playback: systime: 120174019 nsec, audio time 125000000 nsec, 	systime delta -4825981
  playback: systime: 245041136 nsec, audio time 250000000 nsec, 	systime delta -4958864
  playback: systime: 370106088 nsec, audio time 375000000 nsec, 	systime delta -4893912
  playback: systime: 495040065 nsec, audio time 500000000 nsec, 	systime delta -4959935
  playback: systime: 620038179 nsec, audio time 625000000 nsec, 	systime delta -4961821
  playback: systime: 745087741 nsec, audio time 750000000 nsec, 	systime delta -4912259
  playback: systime: 870037336 nsec, audio time 875000000 nsec, 	systime delta -4962664

Example 4: DMA timestamp, compensation for delay, delay of ~1ms
::

  $ ./audio_time -p -Dhw:1 -t1 -d
  playback: systime: 120190520 nsec, audio time 120000000 nsec, 	systime delta 190520
  playback: systime: 245036740 nsec, audio time 244000000 nsec, 	systime delta 1036740
  playback: systime: 370034081 nsec, audio time 369000000 nsec, 	systime delta 1034081
  playback: systime: 495159907 nsec, audio time 494000000 nsec, 	systime delta 1159907
  playback: systime: 620098824 nsec, audio time 619000000 nsec, 	systime delta 1098824
  playback: systime: 745031847 nsec, audio time 744000000 nsec, 	systime delta 1031847
