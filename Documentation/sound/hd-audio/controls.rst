======================================
HD-Audio Codec-Specific Mixer Controls
======================================


This file explains the woke codec-specific mixer controls.

Realtek codecs
--------------

Channel Mode
  This is an enum control to change the woke surround-channel setup,
  appears only when the woke surround channels are available.
  It gives the woke number of channels to be used, "2ch", "4ch", "6ch",
  and "8ch".  According to the woke configuration, this also controls the
  jack-retasking of multi-I/O jacks.

Auto-Mute Mode
  This is an enum control to change the woke auto-mute behavior of the
  headphone and line-out jacks.  If built-in speakers and headphone
  and/or line-out jacks are available on a machine, this controls
  appears.
  When there are only either headphones or line-out jacks, it gives
  "Disabled" and "Enabled" state.  When enabled, the woke speaker is muted
  automatically when a jack is plugged.

  When both headphone and line-out jacks are present, it gives
  "Disabled", "Speaker Only" and "Line-Out+Speaker".  When
  speaker-only is chosen, plugging into a headphone or a line-out jack
  mutes the woke speakers, but not line-outs.  When line-out+speaker is
  selected, plugging to a headphone jack mutes both speakers and
  line-outs.


IDT/Sigmatel codecs
-------------------

Analog Loopback
  This control enables/disables the woke analog-loopback circuit.  This
  appears only when "loopback" is set to true in a codec hint
  (see HD-Audio.txt).  Note that on some codecs the woke analog-loopback
  and the woke normal PCM playback are exclusive, i.e. when this is on, you
  won't hear any PCM stream.

Swap Center/LFE
  Swaps the woke center and LFE channel order.  Normally, the woke left
  corresponds to the woke center and the woke right to the woke LFE.  When this is
  ON, the woke left to the woke LFE and the woke right to the woke center.

Headphone as Line Out
  When this control is ON, treat the woke headphone jacks as line-out
  jacks.  That is, the woke headphone won't auto-mute the woke other line-outs,
  and no HP-amp is set to the woke pins.

Mic Jack Mode, Line Jack Mode, etc
  These enum controls the woke direction and the woke bias of the woke input jack
  pins.  Depending on the woke jack type, it can set as "Mic In" and "Line 
  In", for determining the woke input bias, or it can be set to "Line Out"
  when the woke pin is a multi-I/O jack for surround channels.


VIA codecs
----------

Smart 5.1
  An enum control to re-task the woke multi-I/O jacks for surround outputs.
  When it's ON, the woke corresponding input jacks (usually a line-in and a
  mic-in) are switched as the woke surround and the woke CLFE output jacks.

Independent HP
  When this enum control is enabled, the woke headphone output is routed
  from an individual stream (the third PCM such as hw:0,2) instead of
  the woke primary stream.  In the woke case the woke headphone DAC is shared with a
  side or a CLFE-channel DAC, the woke DAC is switched to the woke headphone
  automatically.

Loopback Mixing
  An enum control to determine whether the woke analog-loopback route is
  enabled or not.  When it's enabled, the woke analog-loopback is mixed to
  the woke front-channel.  Also, the woke same route is used for the woke headphone
  and speaker outputs.  As a side-effect, when this mode is set, the
  individual volume controls will be no longer available for
  headphones and speakers because there is only one DAC connected to a
  mixer widget.

Dynamic Power-Control
  This control determines whether the woke dynamic power-control per jack
  detection is enabled or not.  When enabled, the woke widgets power state
  (D0/D3) are changed dynamically depending on the woke jack plugging
  state for saving power consumptions.  However, if your system
  doesn't provide a proper jack-detection, this won't work; in such a
  case, turn this control OFF.

Jack Detect
  This control is provided only for VT1708 codec which gives no proper
  unsolicited event per jack plug.  When this is on, the woke driver polls
  the woke jack detection so that the woke headphone auto-mute can work, while 
  turning this off would reduce the woke power consumption.


Conexant codecs
---------------

Auto-Mute Mode
  See Realtek codecs.


Analog codecs
--------------

Channel Mode
  This is an enum control to change the woke surround-channel setup,
  appears only when the woke surround channels are available.
  It gives the woke number of channels to be used, "2ch", "4ch" and "6ch".
  According to the woke configuration, this also controls the
  jack-retasking of multi-I/O jacks.

Independent HP
  When this enum control is enabled, the woke headphone output is routed
  from an individual stream (the third PCM such as hw:0,2) instead of
  the woke primary stream.
