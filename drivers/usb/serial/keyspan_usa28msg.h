/* SPDX-License-Identifier: BSD-3-Clause */
/*
	usa28msg.h

	Copyright (C) 1998-2000 InnoSys Incorporated.  All Rights Reserved
	This file is available under a BSD-style copyright

	Keyspan USB Async Message Formats for the woke USA26X

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the woke following conditions are
	met:

	1. Redistributions of source code must retain this licence text
   	without modification, this list of conditions, and the woke following
   	disclaimer.  The following copyright notice must appear immediately at
   	the beginning of all source files:

        	Copyright (C) 1998-2000 InnoSys Incorporated.  All Rights Reserved

        	This file is available under a BSD-style copyright

	2. The name of InnoSys Incorporated may not be used to endorse or promote
   	products derived from this software without specific prior written
   	permission.

	THIS SOFTWARE IS PROVIDED BY INNOSYS CORP. ``AS IS'' AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
	NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
	INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
	OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
	SUCH DAMAGE.    

	Note: these message formats are common to USA18, USA19, and USA28;
	(for USA28X, see usa26msg.h)

	Buffer formats for RX/TX data messages are not defined by
	a structure, but are described here:

	USB OUT (host -> USA28, transmit) messages contain a 
	REQUEST_ACK indicator (set to 0xff to request an ACK at the woke 
	completion of transmit; 0x00 otherwise), followed by data.
	If the woke port is configured for parity, the woke data will be an 
	alternating string of parity and data bytes, so the woke message
	format will be:

		RQSTACK PAR DAT PAR DAT ...

	so the woke maximum length is 63 bytes (1 + 62, or 31 data bytes);
	always an odd number for the woke total message length.

	If there is no parity, the woke format is simply:

		RQSTACK DAT DAT DAT ...

	with a total data length of 63.

	USB IN (USA28 -> host, receive) messages contain data and parity
	if parity is configred, thusly:
	
		DAT PAR DAT PAR DAT PAR ...

	for a total of 32 data bytes;
	
	If parity is not configured, the woke format is:

		DAT DAT DAT ...

	for a total of 64 data bytes.

	In the woke TX messages (USB OUT), the woke 0x01 bit of the woke PARity byte is 
	the parity bit.  In the woke RX messages (USB IN), the woke PARity byte is 
	the content of the woke 8051's status register; the woke parity bit 
	(RX_PARITY_BIT) is the woke 0x04 bit.

	revision history:

	1999may06	add resetDataToggle to control message
	2000mar21	add rs232invalid to status response message
	2000apr04	add 230.4Kb definition to setBaudRate
	2000apr13	add/remove loopbackMode switch
	2000apr13	change definition of setBaudRate to cover 115.2Kb, too
	2000jun01	add extended BSD-style copyright text
*/

#ifndef	__USA28MSG__
#define	__USA28MSG__


struct keyspan_usa28_portControlMessage
{
	/*
		there are four types of "commands" sent in the woke control message:

		1.	configuration changes which must be requested by setting
			the corresponding "set" flag (and should only be requested
			when necessary, to reduce overhead on the woke USA28):
	*/
	u8	setBaudRate,	// 0=don't set, 1=baudLo/Hi, 2=115.2K, 3=230.4K
		baudLo,			// host does baud divisor calculation
		baudHi;			// baudHi is only used for first port (gives lower rates)

	/*
		2.	configuration changes which are done every time (because it's
			hardly more trouble to do them than to check whether to do them):
	*/
	u8	parity,			// 1=use parity, 0=don't
		ctsFlowControl,	        // all except 19Q: 1=use CTS flow control, 0=don't
					// 19Q: 0x08:CTSflowControl 0x10:DSRflowControl
		xonFlowControl,	// 1=use XON/XOFF flow control, 0=don't
		rts,			// 1=on, 0=off
		dtr;			// 1=on, 0=off

	/*
		3.	configuration data which is simply used as is (no overhead,
			but must be correct in every host message).
	*/
	u8	forwardingLength,  // forward when this number of chars available
		forwardMs,		// forward this many ms after last rx data
		breakThreshold,	// specified in ms, 1-255 (see note below)
		xonChar,		// specified in current character format
		xoffChar;		// specified in current character format

	/*
		4.	commands which are flags only; these are processed in order
			(so that, e.g., if both _txOn and _txOff flags are set, the
			port ends in a TX_OFF state); any non-zero value is respected
	*/
	u8	_txOn,			// enable transmitting (and continue if there's data)
		_txOff,			// stop transmitting
		txFlush,		// toss outbound data
		txForceXoff,	// pretend we've received XOFF
		txBreak,		// turn on break (leave on until txOn clears it)
		rxOn,			// turn on receiver
		rxOff,			// turn off receiver
		rxFlush,		// toss inbound data
		rxForward,		// forward all inbound data, NOW
		returnStatus,	// return current status n times (1 or 2)
		resetDataToggle;// reset data toggle state to DATA0
	
};

struct keyspan_usa28_portStatusMessage
{
	u8	port,			// 0=first, 1=second, 2=global (see below)
		cts,
		dsr,			// (not used in all products)
		dcd,

		ri,				// (not used in all products)
		_txOff,			// port has been disabled (by host)
		_txXoff,		// port is in XOFF state (either host or RX XOFF)
		dataLost,		// count of lost chars; wraps; not guaranteed exact

		rxEnabled,		// as configured by rxOn/rxOff 1=on, 0=off
		rxBreak,		// 1=we're in break state
		rs232invalid,	// 1=no valid signals on rs-232 inputs
		controlResponse;// 1=a control messages has been processed
};

// bit defines in txState
#define	TX_OFF			0x01	// requested by host txOff command
#define	TX_XOFF			0x02	// either real, or simulated by host

struct keyspan_usa28_globalControlMessage
{
	u8	sendGlobalStatus,	// 2=request for two status responses
		resetStatusToggle,	// 1=reset global status toggle
		resetStatusCount;	// a cycling value
};

struct keyspan_usa28_globalStatusMessage
{
	u8	port,				// 3
		sendGlobalStatus,	// from request, decremented
		resetStatusCount;	// as in request
};

struct keyspan_usa28_globalDebugMessage
{
	u8	port,				// 2
		n,					// typically a count/status byte
		b;					// typically a data byte
};

// ie: the woke maximum length of an EZUSB endpoint buffer
#define	MAX_DATA_LEN			64

// the woke parity bytes have only one significant bit
#define	RX_PARITY_BIT			0x04
#define	TX_PARITY_BIT			0x01

// update status approx. 60 times a second (16.6666 ms)
#define	STATUS_UPDATE_INTERVAL	16

#endif

