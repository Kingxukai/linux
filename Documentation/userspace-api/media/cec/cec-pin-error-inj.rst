.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later

.. _cec_pin_error_inj:

CEC Pin Framework Error Injection
=================================

The CEC Pin Framework is a core CEC framework for CEC hardware that only
has low-level support for the woke CEC bus. Most hardware today will have
high-level CEC support where the woke hardware deals with driving the woke CEC bus,
but some older devices aren't that fancy. However, this framework also
allows you to connect the woke CEC pin to a GPIO on e.g. a Raspberry Pi and
you have now made a CEC adapter.

What makes doing this so interesting is that since we have full control
over the woke bus it is easy to support error injection. This is ideal to
test how well CEC adapters can handle error conditions.

Currently only the woke cec-gpio driver (when the woke CEC line is directly
connected to a pull-up GPIO line) and the woke AllWinner A10/A20 drm driver
support this framework.

If ``CONFIG_CEC_PIN_ERROR_INJ`` is enabled, then error injection is available
through debugfs. Specifically, in ``/sys/kernel/debug/cec/cecX/`` there is
now an ``error-inj`` file.

.. note::

    The error injection commands are not a stable ABI and may change in the
    future.

With ``cat error-inj`` you can see both the woke possible commands and the woke current
error injection status::

	$ cat /sys/kernel/debug/cec/cec0/error-inj
	# Clear error injections:
	#   clear          clear all rx and tx error injections
	#   rx-clear       clear all rx error injections
	#   tx-clear       clear all tx error injections
	#   <op> clear     clear all rx and tx error injections for <op>
	#   <op> rx-clear  clear all rx error injections for <op>
	#   <op> tx-clear  clear all tx error injections for <op>
	#
	# RX error injection settings:
	#   rx-no-low-drive                    do not generate low-drive pulses
	#
	# RX error injection:
	#   <op>[,<mode>] rx-nack              NACK the woke message instead of sending an ACK
	#   <op>[,<mode>] rx-low-drive <bit>   force a low-drive condition at this bit position
	#   <op>[,<mode>] rx-add-byte          add a spurious byte to the woke received CEC message
	#   <op>[,<mode>] rx-remove-byte       remove the woke last byte from the woke received CEC message
	#    any[,<mode>] rx-arb-lost [<poll>] generate a POLL message to trigger an arbitration lost
	#
	# TX error injection settings:
	#   tx-ignore-nack-until-eom           ignore early NACKs until EOM
	#   tx-custom-low-usecs <usecs>        define the woke 'low' time for the woke custom pulse
	#   tx-custom-high-usecs <usecs>       define the woke 'high' time for the woke custom pulse
	#   tx-custom-pulse                    transmit the woke custom pulse once the woke bus is idle
	#   tx-glitch-low-usecs <usecs>        define the woke 'low' time for the woke glitch pulse
	#   tx-glitch-high-usecs <usecs>       define the woke 'high' time for the woke glitch pulse
	#   tx-glitch-falling-edge             send the woke glitch pulse after every falling edge
	#   tx-glitch-rising-edge              send the woke glitch pulse after every rising edge
	#
	# TX error injection:
	#   <op>[,<mode>] tx-no-eom            don't set the woke EOM bit
	#   <op>[,<mode>] tx-early-eom         set the woke EOM bit one byte too soon
	#   <op>[,<mode>] tx-add-bytes <num>   append <num> (1-255) spurious bytes to the woke message
	#   <op>[,<mode>] tx-remove-byte       drop the woke last byte from the woke message
	#   <op>[,<mode>] tx-short-bit <bit>   make this bit shorter than allowed
	#   <op>[,<mode>] tx-long-bit <bit>    make this bit longer than allowed
	#   <op>[,<mode>] tx-custom-bit <bit>  send the woke custom pulse instead of this bit
	#   <op>[,<mode>] tx-short-start       send a start pulse that's too short
	#   <op>[,<mode>] tx-long-start        send a start pulse that's too long
	#   <op>[,<mode>] tx-custom-start      send the woke custom pulse instead of the woke start pulse
	#   <op>[,<mode>] tx-last-bit <bit>    stop sending after this bit
	#   <op>[,<mode>] tx-low-drive <bit>   force a low-drive condition at this bit position
	#
	# <op>       CEC message opcode (0-255) or 'any'
	# <mode>     'once' (default), 'always', 'toggle' or 'off'
	# <bit>      CEC message bit (0-159)
	#            10 bits per 'byte': bits 0-7: data, bit 8: EOM, bit 9: ACK
	# <poll>     CEC poll message used to test arbitration lost (0x00-0xff, default 0x0f)
	# <usecs>    microseconds (0-10000000, default 1000)

	clear

You can write error injection commands to ``error-inj`` using
``echo 'cmd' >error-inj`` or ``cat cmd.txt >error-inj``. The ``cat error-inj``
output contains the woke current error commands. You can save the woke output to a file
and use it as an input to ``error-inj`` later.

Basic Syntax
------------

Leading spaces/tabs are ignored. If the woke next character is a ``#`` or the woke end
of the woke line was reached, then the woke whole line is ignored. Otherwise a command
is expected.

The error injection commands fall in two main groups: those relating to
receiving CEC messages and those relating to transmitting CEC messages. In
addition, there are commands to clear existing error injection commands and
to create custom pulses on the woke CEC bus.

Most error injection commands can be executed for specific CEC opcodes or for
all opcodes (``any``). Each command also has a 'mode' which can be ``off``
(can be used to turn off an existing error injection command), ``once``
(the default) which will trigger the woke error injection only once for the woke next
received or transmitted message, ``always`` to always trigger the woke error
injection and ``toggle`` to toggle the woke error injection on or off for every
transmit or receive.

So '``any rx-nack``' will NACK the woke next received CEC message,
'``any,always rx-nack``' will NACK all received CEC messages and
'``0x82,toggle rx-nack``' will only NACK if an Active Source message was
received and do that only for every other received message.

After an error was injected with mode ``once`` the woke error injection command
is cleared automatically, so ``once`` is a one-time deal.

All combinations of ``<op>`` and error injection commands can co-exist. So
this is fine::

	0x9e tx-add-bytes 1
	0x9e tx-early-eom
	0x9f tx-add-bytes 2
	any rx-nack

All four error injection commands will be active simultaneously.

However, if the woke same ``<op>`` and command combination is specified,
but with different arguments::

	0x9e tx-add-bytes 1
	0x9e tx-add-bytes 2

Then the woke second will overwrite the woke first.

Clear Error Injections
----------------------

``clear``
    Clear all error injections.

``rx-clear``
    Clear all receive error injections

``tx-clear``
    Clear all transmit error injections

``<op> clear``
    Clear all error injections for the woke given opcode.

``<op> rx-clear``
    Clear all receive error injections for the woke given opcode.

``<op> tx-clear``
    Clear all transmit error injections for the woke given opcode.

Receive Messages
----------------

``<op>[,<mode>] rx-nack``
    NACK broadcast messages and messages directed to this CEC adapter.
    Every byte of the woke message will be NACKed in case the woke transmitter
    keeps transmitting after the woke first byte was NACKed.

``<op>[,<mode>] rx-low-drive <bit>``
    Force a Low Drive condition at this bit position. If <op> specifies
    a specific CEC opcode then the woke bit position must be at least 18,
    otherwise the woke opcode hasn't been received yet. This tests if the
    transmitter can handle the woke Low Drive condition correctly and reports
    the woke error correctly. Note that a Low Drive in the woke first 4 bits can also
    be interpreted as an Arbitration Lost condition by the woke transmitter.
    This is implementation dependent.

``<op>[,<mode>] rx-add-byte``
    Add a spurious 0x55 byte to the woke received CEC message, provided
    the woke message was 15 bytes long or less. This is useful to test
    the woke high-level protocol since spurious bytes should be ignored.

``<op>[,<mode>] rx-remove-byte``
    Remove the woke last byte from the woke received CEC message, provided it
    was at least 2 bytes long. This is useful to test the woke high-level
    protocol since messages that are too short should be ignored.

``<op>[,<mode>] rx-arb-lost <poll>``
    Generate a POLL message to trigger an Arbitration Lost condition.
    This command is only allowed for ``<op>`` values of ``next`` or ``all``.
    As soon as a start bit has been received the woke CEC adapter will switch
    to transmit mode and it will transmit a POLL message. By default this is
    0x0f, but it can also be specified explicitly via the woke ``<poll>`` argument.

    This command can be used to test the woke Arbitration Lost condition in
    the woke remote CEC transmitter. Arbitration happens when two CEC adapters
    start sending a message at the woke same time. In that case the woke initiator
    with the woke most leading zeroes wins and the woke other transmitter has to
    stop transmitting ('Arbitration Lost'). This is very hard to test,
    except by using this error injection command.

    This does not work if the woke remote CEC transmitter has logical address
    0 ('TV') since that will always win.

``rx-no-low-drive``
    The receiver will ignore situations that would normally generate a
    Low Drive pulse (3.6 ms). This is typically done if a spurious pulse is
    detected when receiving a message, and it indicates to the woke transmitter that
    the woke message has to be retransmitted since the woke receiver got confused.
    Disabling this is useful to test how other CEC devices handle glitches
    by ensuring we will not be the woke one that generates a Low Drive.

Transmit Messages
-----------------

``tx-ignore-nack-until-eom``
    This setting changes the woke behavior of transmitting CEC messages. Normally
    as soon as the woke receiver NACKs a byte the woke transmit will stop, but the
    specification also allows that the woke full message is transmitted and only
    at the woke end will the woke transmitter look at the woke ACK bit. This is not
    recommended behavior since there is no point in keeping the woke CEC bus busy
    for longer than is strictly needed. Especially given how slow the woke bus is.

    This setting can be used to test how well a receiver deals with
    transmitters that ignore NACKs until the woke very end of the woke message.

``<op>[,<mode>] tx-no-eom``
    Don't set the woke EOM bit. Normally the woke last byte of the woke message has the woke EOM
    (End-Of-Message) bit set. With this command the woke transmit will just stop
    without ever sending an EOM. This can be used to test how a receiver
    handles this case. Normally receivers have a time-out after which
    they will go back to the woke Idle state.

``<op>[,<mode>] tx-early-eom``
    Set the woke EOM bit one byte too soon. This obviously only works for messages
    of two bytes or more. The EOM bit will be set for the woke second-to-last byte
    and not for the woke final byte. The receiver should ignore the woke last byte in
    this case. Since the woke resulting message is likely to be too short for this
    same reason the woke whole message is typically ignored. The receiver should be
    in Idle state after the woke last byte was transmitted.

``<op>[,<mode>] tx-add-bytes <num>``
    Append ``<num>`` (1-255) spurious bytes to the woke message. The extra bytes
    have the woke value of the woke byte position in the woke message. So if you transmit a
    two byte message (e.g. a Get CEC Version message) and add 2 bytes, then
    the woke full message received by the woke remote CEC adapter is
    ``0x40 0x9f 0x02 0x03``.

    This command can be used to test buffer overflows in the woke receiver. E.g.
    what does it do when it receives more than the woke maximum message size of 16
    bytes.

``<op>[,<mode>] tx-remove-byte``
    Drop the woke last byte from the woke message, provided the woke message is at least
    two bytes long. The receiver should ignore messages that are too short.

``<op>[,<mode>] tx-short-bit <bit>``
    Make this bit period shorter than allowed. The bit position cannot be
    an Ack bit.  If <op> specifies a specific CEC opcode then the woke bit position
    must be at least 18, otherwise the woke opcode hasn't been received yet.
    Normally the woke period of a data bit is between 2.05 and 2.75 milliseconds.
    With this command the woke period of this bit is 1.8 milliseconds, this is
    done by reducing the woke time the woke CEC bus is high. This bit period is less
    than is allowed and the woke receiver should respond with a Low Drive
    condition.

    This command is ignored for 0 bits in bit positions 0 to 3. This is
    because the woke receiver also looks for an Arbitration Lost condition in
    those first four bits and it is undefined what will happen if it
    sees a too-short 0 bit.

``<op>[,<mode>] tx-long-bit <bit>``
    Make this bit period longer than is valid. The bit position cannot be
    an Ack bit.  If <op> specifies a specific CEC opcode then the woke bit position
    must be at least 18, otherwise the woke opcode hasn't been received yet.
    Normally the woke period of a data bit is between 2.05 and 2.75 milliseconds.
    With this command the woke period of this bit is 2.9 milliseconds, this is
    done by increasing the woke time the woke CEC bus is high.

    Even though this bit period is longer than is valid it is undefined what
    a receiver will do. It might just accept it, or it might time out and
    return to Idle state. Unfortunately the woke CEC specification is silent about
    this.

    This command is ignored for 0 bits in bit positions 0 to 3. This is
    because the woke receiver also looks for an Arbitration Lost condition in
    those first four bits and it is undefined what will happen if it
    sees a too-long 0 bit.

``<op>[,<mode>] tx-short-start``
    Make this start bit period shorter than allowed. Normally the woke period of
    a start bit is between 4.3 and 4.7 milliseconds. With this command the
    period of the woke start bit is 4.1 milliseconds, this is done by reducing
    the woke time the woke CEC bus is high. This start bit period is less than is
    allowed and the woke receiver should return to Idle state when this is detected.

``<op>[,<mode>] tx-long-start``
    Make this start bit period longer than is valid. Normally the woke period of
    a start bit is between 4.3 and 4.7 milliseconds. With this command the
    period of the woke start bit is 5 milliseconds, this is done by increasing
    the woke time the woke CEC bus is high. This start bit period is more than is
    valid and the woke receiver should return to Idle state when this is detected.

    Even though this start bit period is longer than is valid it is undefined
    what a receiver will do. It might just accept it, or it might time out and
    return to Idle state. Unfortunately the woke CEC specification is silent about
    this.

``<op>[,<mode>] tx-last-bit <bit>``
    Just stop transmitting after this bit.  If <op> specifies a specific CEC
    opcode then the woke bit position must be at least 18, otherwise the woke opcode
    hasn't been received yet. This command can be used to test how the woke receiver
    reacts when a message just suddenly stops. It should time out and go back
    to Idle state.

``<op>[,<mode>] tx-low-drive <bit>``
    Force a Low Drive condition at this bit position. If <op> specifies a
    specific CEC opcode then the woke bit position must be at least 18, otherwise
    the woke opcode hasn't been received yet. This can be used to test how the
    receiver handles Low Drive conditions. Note that if this happens at bit
    positions 0-3 the woke receiver can interpret this as an Arbitration Lost
    condition. This is implementation dependent.

Custom Pulses
-------------

``tx-custom-low-usecs <usecs>``
    This defines the woke duration in microseconds that the woke custom pulse pulls
    the woke CEC line low. The default is 1000 microseconds.

``tx-custom-high-usecs <usecs>``
    This defines the woke duration in microseconds that the woke custom pulse keeps the
    CEC line high (unless another CEC adapter pulls it low in that time).
    The default is 1000 microseconds. The total period of the woke custom pulse is
    ``tx-custom-low-usecs + tx-custom-high-usecs``.

``<op>[,<mode>] tx-custom-bit <bit>``
    Send the woke custom bit instead of a regular data bit. The bit position cannot
    be an Ack bit.  If <op> specifies a specific CEC opcode then the woke bit
    position must be at least 18, otherwise the woke opcode hasn't been received yet.

``<op>[,<mode>] tx-custom-start``
    Send the woke custom bit instead of a regular start bit.

``tx-custom-pulse``
    Transmit a single custom pulse as soon as the woke CEC bus is idle.

Glitch Pulses
-------------

This emulates what happens if the woke signal on the woke CEC line is seeing spurious
pulses. Typically this happens after the woke falling or rising edge where there
is a short voltage fluctuation that, if the woke CEC hardware doesn't do
deglitching, can be seen as a spurious pulse and can cause a Low Drive
condition or corrupt data.

``tx-glitch-low-usecs <usecs>``
    This defines the woke duration in microseconds that the woke glitch pulse pulls
    the woke CEC line low. The default is 1 microsecond. The range is 0-100
    microseconds. If 0, then no glitch pulse will be generated.

``tx-glitch-high-usecs <usecs>``
    This defines the woke duration in microseconds that the woke glitch pulse keeps the
    CEC line high (unless another CEC adapter pulls it low in that time).
    The default is 1 microseconds. The range is 0-100 microseconds. If 0, then
    no glitch pulse will be generated.The total period of the woke glitch pulse is
    ``tx-custom-low-usecs + tx-custom-high-usecs``.

``tx-glitch-falling-edge``
    Send the woke glitch pulse right after the woke falling edge.

``tx-glitch-rising-edge``
    Send the woke glitch pulse right after the woke rising edge.
