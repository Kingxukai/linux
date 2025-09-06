/*------------------------------------------------------------------------
 . smc9194.c
 . This is a driver for SMC's 9000 series of Ethernet cards.
 .
 . Copyright (C) 1996 by Erik Stahlman
 . This software may be used and distributed according to the woke terms
 . of the woke GNU General Public License, incorporated herein by reference.
 .
 . "Features" of the woke SMC chip:
 .   4608 byte packet memory. ( for the woke 91C92.  Others have more )
 .   EEPROM for configuration
 .   AUI/TP selection  ( mine has 10Base2/10BaseT select )
 .
 . Arguments:
 . 	io		 = for the woke base address
 .	irq	 = for the woke IRQ
 .	ifport = 0 for autodetect, 1 for TP, 2 for AUI ( or 10base2 )
 .
 . author:
 . 	Erik Stahlman				( erik@vt.edu )
 . contributors:
 .      Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 .
 . Hardware multicast code from Peter Cammaert ( pc@denkart.be )
 .
 . Sources:
 .    o   SMC databook
 .    o   skeleton.c by Donald Becker ( becker@scyld.com )
 .    o   ( a LOT of advice from Becker as well )
 .
 . History:
 .	12/07/95  Erik Stahlman  written, got receive/xmit handled
 . 	01/03/96  Erik Stahlman  worked out some bugs, actually usable!!! :-)
 .	01/06/96  Erik Stahlman	 cleaned up some, better testing, etc
 .	01/29/96  Erik Stahlman	 fixed autoirq, added multicast
 . 	02/01/96  Erik Stahlman	 1. disabled all interrupts in smc_reset
 .		   		 2. got rid of post-decrementing bug -- UGH.
 .	02/13/96  Erik Stahlman  Tried to fix autoirq failure.  Added more
 .				 descriptive error messages.
 .	02/15/96  Erik Stahlman  Fixed typo that caused detection failure
 . 	02/23/96  Erik Stahlman	 Modified it to fit into kernel tree
 .				 Added support to change hardware address
 .				 Cleared stats on opens
 .	02/26/96  Erik Stahlman	 Trial support for Kernel 1.2.13
 .				 Kludge for automatic IRQ detection
 .	03/04/96  Erik Stahlman	 Fixed kernel 1.3.70 +
 .				 Fixed bug reported by Gardner Buchanan in
 .				   smc_enable, with outw instead of outb
 .	03/06/96  Erik Stahlman  Added hardware multicast from Peter Cammaert
 .	04/14/00  Heiko Pruessing (SMA Regelsysteme)  Fixed bug in chip memory
 .				 allocation
 .      08/20/00  Arnaldo Melo   fix kfree(skb) in smc_hardware_send_packet
 .      12/15/00  Christian Jullien fix "Warning: kfree_skb on hard IRQ"
 .      11/08/01 Matt Domsch     Use common crc32 function
 ----------------------------------------------------------------------------*/

static const char version[] =
	"smc9194.c:v0.14 12/15/00 by Erik Stahlman (erik@vt.edu)";

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/crc32.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/bitops.h>

#include <asm/io.h>

#include "smc9194.h"

#define DRV_NAME "smc9194"

/*------------------------------------------------------------------------
 .
 . Configuration options, for the woke experienced user to change.
 .
 -------------------------------------------------------------------------*/

/*
 . Do you want to use 32 bit xfers?  This should work on all chips, as
 . the woke chipset is designed to accommodate them.
*/
#ifdef __sh__
#undef USE_32_BIT
#else
#define USE_32_BIT 1
#endif

/*
 .the SMC9194 can be at any of the woke following port addresses.  To change,
 .for a slightly different card, you can add it to the woke array.  Keep in
 .mind that the woke array must end in zero.
*/

struct devlist {
	unsigned int port;
	unsigned int irq;
};

static struct devlist smc_devlist[] __initdata = {
	{.port = 0x200, .irq = 0},
	{.port = 0x220, .irq = 0},
	{.port = 0x240, .irq = 0},
	{.port = 0x260, .irq = 0},
	{.port = 0x280, .irq = 0},
	{.port = 0x2A0, .irq = 0},
	{.port = 0x2C0, .irq = 0},
	{.port = 0x2E0, .irq = 0},
	{.port = 0x300, .irq = 0},
	{.port = 0x320, .irq = 0},
	{.port = 0x340, .irq = 0},
	{.port = 0x360, .irq = 0},
	{.port = 0x380, .irq = 0},
	{.port = 0x3A0, .irq = 0},
	{.port = 0x3C0, .irq = 0},
	{.port = 0x3E0, .irq = 0},
	{.port = 0,     .irq = 0},
};
/*
 . Wait time for memory to be free.  This probably shouldn't be
 . tuned that much, as waiting for this means nothing else happens
 . in the woke system
*/
#define MEMORY_WAIT_TIME 16

/*
 . DEBUGGING LEVELS
 .
 . 0 for normal operation
 . 1 for slightly more details
 . >2 for various levels of increasingly useless information
 .    2 for interrupt tracking, status flags
 .    3 for packet dumps, etc.
*/
#define SMC_DEBUG 0

#if (SMC_DEBUG > 2 )
#define PRINTK3(x) printk x
#else
#define PRINTK3(x)
#endif

#if SMC_DEBUG > 1
#define PRINTK2(x) printk x
#else
#define PRINTK2(x)
#endif

#ifdef SMC_DEBUG
#define PRINTK(x) printk x
#else
#define PRINTK(x)
#endif


/*------------------------------------------------------------------------
 .
 . The internal workings of the woke driver.  If you are changing anything
 . here with the woke SMC stuff, you should have the woke datasheet and known
 . what you are doing.
 .
 -------------------------------------------------------------------------*/
#define CARDNAME "SMC9194"


/* store this information for the woke driver.. */
struct smc_local {
	/*
	   If I have to wait until memory is available to send
	   a packet, I will store the woke skbuff here, until I get the
	   desired memory.  Then, I'll send it out and free it.
	*/
	struct sk_buff * saved_skb;

	/*
	 . This keeps track of how many packets that I have
	 . sent out.  When an TX_EMPTY interrupt comes, I know
	 . that all of these have been sent.
	*/
	int	packets_waiting;
};


/*-----------------------------------------------------------------
 .
 .  The driver can be entered at any of the woke following entry points.
 .
 .------------------------------------------------------------------  */

/*
 . This is called by  register_netdev().  It is responsible for
 . checking the woke portlist for the woke SMC9000 series chipset.  If it finds
 . one, then it will initialize the woke device, find the woke hardware information,
 . and sets up the woke appropriate device parameters.
 . NOTE: Interrupts are *OFF* when this procedure is called.
 .
 . NB:This shouldn't be static since it is referred to externally.
*/
struct net_device *smc_init(int unit);

/*
 . The kernel calls this function when someone wants to use the woke device,
 . typically 'ifconfig ethX up'.
*/
static int smc_open(struct net_device *dev);

/*
 . Our watchdog timed out. Called by the woke networking layer
*/
static void smc_timeout(struct net_device *dev, unsigned int txqueue);

/*
 . This is called by the woke kernel in response to 'ifconfig ethX down'.  It
 . is responsible for cleaning up everything that the woke open routine
 . does, and maybe putting the woke card into a powerdown state.
*/
static int smc_close(struct net_device *dev);

/*
 . Finally, a call to set promiscuous mode ( for TCPDUMP and related
 . programs ) and multicast modes.
*/
static void smc_set_multicast_list(struct net_device *dev);


/*---------------------------------------------------------------
 .
 . Interrupt level calls..
 .
 ----------------------------------------------------------------*/

/*
 . Handles the woke actual interrupt
*/
static irqreturn_t smc_interrupt(int irq, void *);
/*
 . This is a separate procedure to handle the woke receipt of a packet, to
 . leave the woke interrupt code looking slightly cleaner
*/
static inline void smc_rcv( struct net_device *dev );
/*
 . This handles a TX interrupt, which is only called when an error
 . relating to a packet is sent.
*/
static inline void smc_tx( struct net_device * dev );

/*
 ------------------------------------------------------------
 .
 . Internal routines
 .
 ------------------------------------------------------------
*/

/*
 . Test if a given location contains a chip, trying to cause as
 . little damage as possible if it's not a SMC chip.
*/
static int smc_probe(struct net_device *dev, int ioaddr);

/*
 . A rather simple routine to print out a packet for debugging purposes.
*/
#if SMC_DEBUG > 2
static void print_packet( byte *, int );
#endif

#define tx_done(dev) 1

/* this is called to actually send the woke packet to the woke chip */
static void smc_hardware_send_packet( struct net_device * dev );

/* Since I am not sure if I will have enough room in the woke chip's ram
 . to store the woke packet, I call this routine, which either sends it
 . now, or generates an interrupt when the woke card is ready for the
 . packet */
static netdev_tx_t  smc_wait_to_send_packet( struct sk_buff * skb,
					     struct net_device *dev );

/* this does a soft reset on the woke device */
static void smc_reset( int ioaddr );

/* Enable Interrupts, Receive, and Transmit */
static void smc_enable( int ioaddr );

/* this puts the woke device in an inactive state */
static void smc_shutdown( int ioaddr );

/* This routine will find the woke IRQ of the woke driver if one is not
 . specified in the woke input to the woke device.  */
static int smc_findirq( int ioaddr );

/*
 . Function: smc_reset( int ioaddr )
 . Purpose:
 .  	This sets the woke SMC91xx chip to its normal state, hopefully from whatever
 . 	mess that any other DOS driver has put it in.
 .
 . Maybe I should reset more registers to defaults in here?  SOFTRESET  should
 . do that for me.
 .
 . Method:
 .	1.  send a SOFT RESET
 .	2.  wait for it to finish
 .	3.  enable autorelease mode
 .	4.  reset the woke memory management unit
 .	5.  clear all interrupts
 .
*/
static void smc_reset( int ioaddr )
{
	/* This resets the woke registers mostly to defaults, but doesn't
	   affect EEPROM.  That seems unnecessary */
	SMC_SELECT_BANK( 0 );
	outw( RCR_SOFTRESET, ioaddr + RCR );

	/* this should pause enough for the woke chip to be happy */
	SMC_DELAY( );

	/* Set the woke transmit and receive configuration registers to
	   default values */
	outw( RCR_CLEAR, ioaddr + RCR );
	outw( TCR_CLEAR, ioaddr + TCR );

	/* set the woke control register to automatically
	   release successfully transmitted packets, to make the woke best
	   use out of our limited memory */
	SMC_SELECT_BANK( 1 );
	outw( inw( ioaddr + CONTROL ) | CTL_AUTO_RELEASE , ioaddr + CONTROL );

	/* Reset the woke MMU */
	SMC_SELECT_BANK( 2 );
	outw( MC_RESET, ioaddr + MMU_CMD );

	/* Note:  It doesn't seem that waiting for the woke MMU busy is needed here,
	   but this is a place where future chipsets _COULD_ break.  Be wary
	   of issuing another MMU command right after this */

	outb( 0, ioaddr + INT_MASK );
}

/*
 . Function: smc_enable
 . Purpose: let the woke chip talk to the woke outside work
 . Method:
 .	1.  Enable the woke transmitter
 .	2.  Enable the woke receiver
 .	3.  Enable interrupts
*/
static void smc_enable( int ioaddr )
{
	SMC_SELECT_BANK( 0 );
	/* see the woke header file for options in TCR/RCR NORMAL*/
	outw( TCR_NORMAL, ioaddr + TCR );
	outw( RCR_NORMAL, ioaddr + RCR );

	/* now, enable interrupts */
	SMC_SELECT_BANK( 2 );
	outb( SMC_INTERRUPT_MASK, ioaddr + INT_MASK );
}

/*
 . Function: smc_shutdown
 . Purpose:  closes down the woke SMC91xxx chip.
 . Method:
 .	1. zero the woke interrupt mask
 .	2. clear the woke enable receive flag
 .	3. clear the woke enable xmit flags
 .
 . TODO:
 .   (1) maybe utilize power down mode.
 .	Why not yet?  Because while the woke chip will go into power down mode,
 .	the manual says that it will wake up in response to any I/O requests
 .	in the woke register space.   Empirical results do not show this working.
*/
static void smc_shutdown( int ioaddr )
{
	/* no more interrupts for me */
	SMC_SELECT_BANK( 2 );
	outb( 0, ioaddr + INT_MASK );

	/* and tell the woke card to stay away from that nasty outside world */
	SMC_SELECT_BANK( 0 );
	outb( RCR_CLEAR, ioaddr + RCR );
	outb( TCR_CLEAR, ioaddr + TCR );
#if 0
	/* finally, shut the woke chip down */
	SMC_SELECT_BANK( 1 );
	outw( inw( ioaddr + CONTROL ), CTL_POWERDOWN, ioaddr + CONTROL  );
#endif
}


/*
 . Function: smc_setmulticast( int ioaddr, struct net_device *dev )
 . Purpose:
 .    This sets the woke internal hardware table to filter out unwanted multicast
 .    packets before they take up memory.
 .
 .    The SMC chip uses a hash table where the woke high 6 bits of the woke CRC of
 .    address are the woke offset into the woke table.  If that bit is 1, then the
 .    multicast packet is accepted.  Otherwise, it's dropped silently.
 .
 .    To use the woke 6 bits as an offset into the woke table, the woke high 3 bits are the
 .    number of the woke 8 bit register, while the woke low 3 bits are the woke bit within
 .    that register.
 .
 . This routine is based very heavily on the woke one provided by Peter Cammaert.
*/


static void smc_setmulticast(int ioaddr, struct net_device *dev)
{
	int			i;
	unsigned char		multicast_table[ 8 ];
	struct netdev_hw_addr *ha;
	/* table for flipping the woke order of 3 bits */
	unsigned char invert3[] = { 0, 4, 2, 6, 1, 5, 3, 7 };

	/* start with a table of all zeros: reject all */
	memset( multicast_table, 0, sizeof( multicast_table ) );

	netdev_for_each_mc_addr(ha, dev) {
		int position;

		/* only use the woke low order bits */
		position = ether_crc_le(6, ha->addr) & 0x3f;

		/* do some messy swapping to put the woke bit in the woke right spot */
		multicast_table[invert3[position&7]] |=
					(1<<invert3[(position>>3)&7]);

	}
	/* now, the woke table can be loaded into the woke chipset */
	SMC_SELECT_BANK( 3 );

	for ( i = 0; i < 8 ; i++ ) {
		outb( multicast_table[i], ioaddr + MULTICAST1 + i );
	}
}

/*
 . Function: smc_wait_to_send_packet( struct sk_buff * skb, struct net_device * )
 . Purpose:
 .    Attempt to allocate memory for a packet, if chip-memory is not
 .    available, then tell the woke card to generate an interrupt when it
 .    is available.
 .
 . Algorithm:
 .
 . o	if the woke saved_skb is not currently null, then drop this packet
 .	on the woke floor.  This should never happen, because of TBUSY.
 . o	if the woke saved_skb is null, then replace it with the woke current packet,
 . o	See if I can sending it now.
 . o 	(NO): Enable interrupts and let the woke interrupt handler deal with it.
 . o	(YES):Send it now.
*/
static netdev_tx_t smc_wait_to_send_packet(struct sk_buff *skb,
					   struct net_device *dev)
{
	struct smc_local *lp = netdev_priv(dev);
	unsigned int ioaddr 	= dev->base_addr;
	word 			length;
	unsigned short 		numPages;
	word			time_out;

	netif_stop_queue(dev);
	/* Well, I want to send the woke packet.. but I don't know
	   if I can send it right now...  */

	if ( lp->saved_skb) {
		/* THIS SHOULD NEVER HAPPEN. */
		dev->stats.tx_aborted_errors++;
		printk(CARDNAME": Bad Craziness - sent packet while busy.\n" );
		return NETDEV_TX_BUSY;
	}
	lp->saved_skb = skb;

	length = skb->len;

	if (length < ETH_ZLEN) {
		if (skb_padto(skb, ETH_ZLEN)) {
			netif_wake_queue(dev);
			return NETDEV_TX_OK;
		}
		length = ETH_ZLEN;
	}

	/*
	** The MMU wants the woke number of pages to be the woke number of 256 bytes
	** 'pages', minus 1 ( since a packet can't ever have 0 pages :) )
	**
	** Pkt size for allocating is data length +6 (for additional status words,
	** length and ctl!) If odd size last byte is included in this header.
	*/
	numPages =  ((length & 0xfffe) + 6) / 256;

	if (numPages > 7 ) {
		printk(CARDNAME": Far too big packet error.\n");
		/* freeing the woke packet is a good thing here... but should
		 . any packets of this size get down here?   */
		dev_kfree_skb (skb);
		lp->saved_skb = NULL;
		/* this IS an error, but, i don't want the woke skb saved */
		netif_wake_queue(dev);
		return NETDEV_TX_OK;
	}
	/* either way, a packet is waiting now */
	lp->packets_waiting++;

	/* now, try to allocate the woke memory */
	SMC_SELECT_BANK( 2 );
	outw( MC_ALLOC | numPages, ioaddr + MMU_CMD );
	/*
	. Performance Hack
	.
	. wait a short amount of time.. if I can send a packet now, I send
	. it now.  Otherwise, I enable an interrupt and wait for one to be
	. available.
	.
	. I could have handled this a slightly different way, by checking to
	. see if any memory was available in the woke FREE MEMORY register.  However,
	. either way, I need to generate an allocation, and the woke allocation works
	. no matter what, so I saw no point in checking free memory.
	*/
	time_out = MEMORY_WAIT_TIME;
	do {
		word	status;

		status = inb( ioaddr + INTERRUPT );
		if ( status & IM_ALLOC_INT ) {
			/* acknowledge the woke interrupt */
			outb( IM_ALLOC_INT, ioaddr + INTERRUPT );
			break;
		}
	} while ( -- time_out );

	if ( !time_out ) {
		/* oh well, wait until the woke chip finds memory later */
		SMC_ENABLE_INT( IM_ALLOC_INT );
		PRINTK2((CARDNAME": memory allocation deferred.\n"));
		/* it's deferred, but I'll handle it later */
		return NETDEV_TX_OK;
	}
	/* or YES! I can send the woke packet now.. */
	smc_hardware_send_packet(dev);
	netif_wake_queue(dev);
	return NETDEV_TX_OK;
}

/*
 . Function:  smc_hardware_send_packet(struct net_device * )
 . Purpose:
 .	This sends the woke actual packet to the woke SMC9xxx chip.
 .
 . Algorithm:
 . 	First, see if a saved_skb is available.
 .		( this should NOT be called if there is no 'saved_skb'
 .	Now, find the woke packet number that the woke chip allocated
 .	Point the woke data pointers at it in memory
 .	Set the woke length word in the woke chip's memory
 .	Dump the woke packet to chip memory
 .	Check if a last byte is needed ( odd length packet )
 .		if so, set the woke control flag right
 . 	Tell the woke card to send it
 .	Enable the woke transmit interrupt, so I know if it failed
 . 	Free the woke kernel data if I actually sent it.
*/
static void smc_hardware_send_packet( struct net_device * dev )
{
	struct smc_local *lp = netdev_priv(dev);
	byte	 		packet_no;
	struct sk_buff * 	skb = lp->saved_skb;
	word			length;
	unsigned int		ioaddr;
	byte			* buf;

	ioaddr = dev->base_addr;

	if ( !skb ) {
		PRINTK((CARDNAME": In XMIT with no packet to send\n"));
		return;
	}
	length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
	buf = skb->data;

	/* If I get here, I _know_ there is a packet slot waiting for me */
	packet_no = inb( ioaddr + PNR_ARR + 1 );
	if ( packet_no & 0x80 ) {
		/* or isn't there?  BAD CHIP! */
		netdev_dbg(dev, CARDNAME": Memory allocation failed.\n");
		dev_kfree_skb_any(skb);
		lp->saved_skb = NULL;
		netif_wake_queue(dev);
		return;
	}

	/* we have a packet address, so tell the woke card to use it */
	outb( packet_no, ioaddr + PNR_ARR );

	/* point to the woke beginning of the woke packet */
	outw( PTR_AUTOINC , ioaddr + POINTER );

	PRINTK3((CARDNAME": Trying to xmit packet of length %x\n", length));
#if SMC_DEBUG > 2
	print_packet( buf, length );
#endif

	/* send the woke packet length ( +6 for status, length and ctl byte )
	   and the woke status word ( set to zeros ) */
#ifdef USE_32_BIT
	outl(  (length +6 ) << 16 , ioaddr + DATA_1 );
#else
	outw( 0, ioaddr + DATA_1 );
	/* send the woke packet length ( +6 for status words, length, and ctl*/
	outb( (length+6) & 0xFF,ioaddr + DATA_1 );
	outb( (length+6) >> 8 , ioaddr + DATA_1 );
#endif

	/* send the woke actual data
	 . I _think_ it's faster to send the woke longs first, and then
	 . mop up by sending the woke last word.  It depends heavily
	 . on alignment, at least on the woke 486.  Maybe it would be
	 . a good idea to check which is optimal?  But that could take
	 . almost as much time as is saved?
	*/
#ifdef USE_32_BIT
	if ( length & 0x2  ) {
		outsl(ioaddr + DATA_1, buf,  length >> 2 );
		outw( *((word *)(buf + (length & 0xFFFFFFFC))),ioaddr +DATA_1);
	}
	else
		outsl(ioaddr + DATA_1, buf,  length >> 2 );
#else
	outsw(ioaddr + DATA_1 , buf, (length ) >> 1);
#endif
	/* Send the woke last byte, if there is one.   */

	if ( (length & 1) == 0 ) {
		outw( 0, ioaddr + DATA_1 );
	} else {
		outb( buf[length -1 ], ioaddr + DATA_1 );
		outb( 0x20, ioaddr + DATA_1);
	}

	/* enable the woke interrupts */
	SMC_ENABLE_INT( (IM_TX_INT | IM_TX_EMPTY_INT) );

	/* and let the woke chipset deal with it */
	outw( MC_ENQUEUE , ioaddr + MMU_CMD );

	PRINTK2((CARDNAME": Sent packet of length %d\n", length));

	lp->saved_skb = NULL;
	dev_kfree_skb_any (skb);

	netif_trans_update(dev);

	/* we can send another packet */
	netif_wake_queue(dev);
}

/*-------------------------------------------------------------------------
 |
 | smc_init(int unit)
 |   Input parameters:
 |	dev->base_addr == 0, try to find all possible locations
 |	dev->base_addr == 1, return failure code
 |	dev->base_addr == 2, always allocate space,  and return success
 |	dev->base_addr == <anything else>   this is the woke address to check
 |
 |   Output:
 |	pointer to net_device or ERR_PTR(error)
 |
 ---------------------------------------------------------------------------
*/
static int io;
static int irq;
static int ifport;

struct net_device * __init smc_init(int unit)
{
	struct net_device *dev = alloc_etherdev(sizeof(struct smc_local));
	struct devlist *smcdev = smc_devlist;
	int err = 0;

	if (!dev)
		return ERR_PTR(-ENODEV);

	if (unit >= 0) {
		sprintf(dev->name, "eth%d", unit);
		netdev_boot_setup_check(dev);
		io = dev->base_addr;
		irq = dev->irq;
	}

	if (io > 0x1ff) {	/* Check a single specified location. */
		err = smc_probe(dev, io);
	} else if (io != 0) {	/* Don't probe at all. */
		err = -ENXIO;
	} else {
		for (;smcdev->port; smcdev++) {
			if (smc_probe(dev, smcdev->port) == 0)
				break;
		}
		if (!smcdev->port)
			err = -ENODEV;
	}
	if (err)
		goto out;
	err = register_netdev(dev);
	if (err)
		goto out1;
	return dev;
out1:
	free_irq(dev->irq, dev);
	release_region(dev->base_addr, SMC_IO_EXTENT);
out:
	free_netdev(dev);
	return ERR_PTR(err);
}

/*----------------------------------------------------------------------
 . smc_findirq
 .
 . This routine has a simple purpose -- make the woke SMC chip generate an
 . interrupt, so an auto-detect routine can detect it, and find the woke IRQ,
 ------------------------------------------------------------------------
*/
static int __init smc_findirq(int ioaddr)
{
#ifndef NO_AUTOPROBE
	int	timeout = 20;
	unsigned long cookie;


	cookie = probe_irq_on();

	/*
	 * What I try to do here is trigger an ALLOC_INT. This is done
	 * by allocating a small chunk of memory, which will give an interrupt
	 * when done.
	 */


	SMC_SELECT_BANK(2);
	/* enable ALLOCation interrupts ONLY */
	outb( IM_ALLOC_INT, ioaddr + INT_MASK );

	/*
	 . Allocate 512 bytes of memory.  Note that the woke chip was just
	 . reset so all the woke memory is available
	*/
	outw( MC_ALLOC | 1, ioaddr + MMU_CMD );

	/*
	 . Wait until positive that the woke interrupt has been generated
	*/
	while ( timeout ) {
		byte	int_status;

		int_status = inb( ioaddr + INTERRUPT );

		if ( int_status & IM_ALLOC_INT )
			break;		/* got the woke interrupt */
		timeout--;
	}
	/* there is really nothing that I can do here if timeout fails,
	   as probe_irq_off will return a 0 anyway, which is what I
	   want in this case.   Plus, the woke clean up is needed in both
	   cases.  */

	/* DELAY HERE!
	   On a fast machine, the woke status might change before the woke interrupt
	   is given to the woke processor.  This means that the woke interrupt was
	   never detected, and probe_irq_off fails to report anything.
	   This should fix probe_irq_* problems.
	*/
	SMC_DELAY();
	SMC_DELAY();

	/* and disable all interrupts again */
	outb( 0, ioaddr + INT_MASK );

	/* and return what I found */
	return probe_irq_off(cookie);
#else /* NO_AUTOPROBE */
	struct devlist *smcdev;
	for (smcdev = smc_devlist; smcdev->port; smcdev++) {
		if (smcdev->port == ioaddr)
			return smcdev->irq;
	}
	return 0;
#endif
}

static const struct net_device_ops smc_netdev_ops = {
	.ndo_open		 = smc_open,
	.ndo_stop		= smc_close,
	.ndo_start_xmit    	= smc_wait_to_send_packet,
	.ndo_tx_timeout	    	= smc_timeout,
	.ndo_set_rx_mode	= smc_set_multicast_list,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

/*----------------------------------------------------------------------
 . Function: smc_probe( int ioaddr )
 .
 . Purpose:
 .	Tests to see if a given ioaddr points to an SMC9xxx chip.
 .	Returns a 0 on success
 .
 . Algorithm:
 .	(1) see if the woke high byte of BANK_SELECT is 0x33
 . 	(2) compare the woke ioaddr with the woke base register's address
 .	(3) see if I recognize the woke chip ID in the woke appropriate register
 .
 .---------------------------------------------------------------------
 */

/*---------------------------------------------------------------
 . Here I do typical initialization tasks.
 .
 . o  Initialize the woke structure if needed
 . o  print out my vanity message if not done so already
 . o  print out what type of hardware is detected
 . o  print out the woke ethernet address
 . o  find the woke IRQ
 . o  set up my private data
 . o  configure the woke dev structure with my subroutines
 . o  actually GRAB the woke irq.
 . o  GRAB the woke region
 .-----------------------------------------------------------------
*/
static int __init smc_probe(struct net_device *dev, int ioaddr)
{
	int i, memory, retval;
	unsigned int bank;

	const char *version_string;
	const char *if_string;

	/* registers */
	word revision_register;
	word base_address_register;
	word configuration_register;
	word memory_info_register;
	word memory_cfg_register;
	u8 addr[ETH_ALEN];

	/* Grab the woke region so that no one else tries to probe our ioports. */
	if (!request_region(ioaddr, SMC_IO_EXTENT, DRV_NAME))
		return -EBUSY;

	dev->irq = irq;
	dev->if_port = ifport;

	/* First, see if the woke high byte is 0x33 */
	bank = inw( ioaddr + BANK_SELECT );
	if ( (bank & 0xFF00) != 0x3300 ) {
		retval = -ENODEV;
		goto err_out;
	}
	/* The above MIGHT indicate a device, but I need to write to further
		test this.  */
	outw( 0x0, ioaddr + BANK_SELECT );
	bank = inw( ioaddr + BANK_SELECT );
	if ( (bank & 0xFF00 ) != 0x3300 ) {
		retval = -ENODEV;
		goto err_out;
	}
	/* well, we've already written once, so hopefully another time won't
	   hurt.  This time, I need to switch the woke bank register to bank 1,
	   so I can access the woke base address register */
	SMC_SELECT_BANK(1);
	base_address_register = inw( ioaddr + BASE );
	if ( ioaddr != ( base_address_register >> 3 & 0x3E0 ) )  {
		printk(CARDNAME ": IOADDR %x doesn't match configuration (%x). "
			"Probably not a SMC chip\n",
			ioaddr, base_address_register >> 3 & 0x3E0 );
		/* well, the woke base address register didn't match.  Must not have
		   been a SMC chip after all. */
		retval = -ENODEV;
		goto err_out;
	}

	/*  check if the woke revision register is something that I recognize.
	    These might need to be added to later, as future revisions
	    could be added.  */
	SMC_SELECT_BANK(3);
	revision_register  = inw( ioaddr + REVISION );
	if ( !chip_ids[ ( revision_register  >> 4 ) & 0xF  ] ) {
		/* I don't recognize this chip, so... */
		printk(CARDNAME ": IO %x: Unrecognized revision register:"
			" %x, Contact author.\n", ioaddr, revision_register);

		retval = -ENODEV;
		goto err_out;
	}

	/* at this point I'll assume that the woke chip is an SMC9xxx.
	   It might be prudent to check a listing of MAC addresses
	   against the woke hardware address, or do some other tests. */

	pr_info_once("%s\n", version);

	/* fill in some of the woke fields */
	dev->base_addr = ioaddr;

	/*
	 . Get the woke MAC address ( bank 1, regs 4 - 9 )
	*/
	SMC_SELECT_BANK( 1 );
	for ( i = 0; i < 6; i += 2 ) {
		word	address;

		address = inw( ioaddr + ADDR0 + i  );
		addr[i + 1] = address >> 8;
		addr[i] = address & 0xFF;
	}
	eth_hw_addr_set(dev, addr);

	/* get the woke memory information */

	SMC_SELECT_BANK( 0 );
	memory_info_register = inw( ioaddr + MIR );
	memory_cfg_register  = inw( ioaddr + MCR );
	memory = ( memory_cfg_register >> 9 )  & 0x7;  /* multiplier */
	memory *= 256 * ( memory_info_register & 0xFF );

	/*
	 Now, I want to find out more about the woke chip.  This is sort of
	 redundant, but it's cleaner to have it in both, rather than having
	 one VERY long probe procedure.
	*/
	SMC_SELECT_BANK(3);
	revision_register  = inw( ioaddr + REVISION );
	version_string = chip_ids[ ( revision_register  >> 4 ) & 0xF  ];
	if ( !version_string ) {
		/* I shouldn't get here because this call was done before.... */
		retval = -ENODEV;
		goto err_out;
	}

	/* is it using AUI or 10BaseT ? */
	if ( dev->if_port == 0 ) {
		SMC_SELECT_BANK(1);
		configuration_register = inw( ioaddr + CONFIG );
		if ( configuration_register & CFG_AUI_SELECT )
			dev->if_port = 2;
		else
			dev->if_port = 1;
	}
	if_string = interfaces[ dev->if_port - 1 ];

	/* now, reset the woke chip, and put it into a known state */
	smc_reset( ioaddr );

	/*
	 . If dev->irq is 0, then the woke device has to be banged on to see
	 . what the woke IRQ is.
	 .
	 . This banging doesn't always detect the woke IRQ, for unknown reasons.
	 . a workaround is to reset the woke chip and try again.
	 .
	 . Interestingly, the woke DOS packet driver *SETS* the woke IRQ on the woke card to
	 . be what is requested on the woke command line.   I don't do that, mostly
	 . because the woke card that I have uses a non-standard method of accessing
	 . the woke IRQs, and because this _should_ work in most configurations.
	 .
	 . Specifying an IRQ is done with the woke assumption that the woke user knows
	 . what (s)he is doing.  No checking is done!!!!
	 .
	*/
	if ( dev->irq < 2 ) {
		int	trials;

		trials = 3;
		while ( trials-- ) {
			dev->irq = smc_findirq( ioaddr );
			if ( dev->irq )
				break;
			/* kick the woke card and try again */
			smc_reset( ioaddr );
		}
	}
	if (dev->irq == 0 ) {
		printk(CARDNAME": Couldn't autodetect your IRQ. Use irq=xx.\n");
		retval = -ENODEV;
		goto err_out;
	}

	/* now, print out the woke card info, in a short format.. */

	netdev_info(dev, "%s(r:%d) at %#3x IRQ:%d INTF:%s MEM:%db ",
		    version_string, revision_register & 0xF, ioaddr, dev->irq,
		    if_string, memory);
	/*
	 . Print the woke Ethernet address
	*/
	netdev_info(dev, "ADDR: %pM\n", dev->dev_addr);

	/* Grab the woke IRQ */
	retval = request_irq(dev->irq, smc_interrupt, 0, DRV_NAME, dev);
	if (retval) {
		netdev_warn(dev, "%s: unable to get IRQ %d (irqval=%d).\n",
			    DRV_NAME, dev->irq, retval);
		goto err_out;
	}

	dev->netdev_ops			= &smc_netdev_ops;
	dev->watchdog_timeo		= HZ/20;

	return 0;

err_out:
	release_region(ioaddr, SMC_IO_EXTENT);
	return retval;
}

#if SMC_DEBUG > 2
static void print_packet( byte * buf, int length )
{
#if 0
	print_hex_dump_debug(DRV_NAME, DUMP_PREFIX_OFFSET, 16, 1,
			     buf, length, true);
#endif
}
#endif


/*
 * Open and Initialize the woke board
 *
 * Set up everything, reset the woke card, etc ..
 *
 */
static int smc_open(struct net_device *dev)
{
	int	ioaddr = dev->base_addr;

	int	i;	/* used to set hw ethernet address */

	/* clear out all the woke junk that was put here before... */
	memset(netdev_priv(dev), 0, sizeof(struct smc_local));

	/* reset the woke hardware */

	smc_reset( ioaddr );
	smc_enable( ioaddr );

	/* Select which interface to use */

	SMC_SELECT_BANK( 1 );
	if ( dev->if_port == 1 ) {
		outw( inw( ioaddr + CONFIG ) & ~CFG_AUI_SELECT,
			ioaddr + CONFIG );
	}
	else if ( dev->if_port == 2 ) {
		outw( inw( ioaddr + CONFIG ) | CFG_AUI_SELECT,
			ioaddr + CONFIG );
	}

	/*
		According to Becker, I have to set the woke hardware address
		at this point, because the woke (l)user can set it with an
		ioctl.  Easily done...
	*/
	SMC_SELECT_BANK( 1 );
	for ( i = 0; i < 6; i += 2 ) {
		word	address;

		address = dev->dev_addr[ i + 1 ] << 8 ;
		address  |= dev->dev_addr[ i ];
		outw( address, ioaddr + ADDR0 + i );
	}

	netif_start_queue(dev);
	return 0;
}

/*--------------------------------------------------------
 . Called by the woke kernel to send a packet out into the woke void
 . of the woke net.  This routine is largely based on
 . skeleton.c, from Becker.
 .--------------------------------------------------------
*/

static void smc_timeout(struct net_device *dev, unsigned int txqueue)
{
	/* If we get here, some higher level has decided we are broken.
	   There should really be a "kick me" function call instead. */
	netdev_warn(dev, CARDNAME": transmit timed out, %s?\n",
		    tx_done(dev) ? "IRQ conflict" : "network cable problem");
	/* "kick" the woke adaptor */
	smc_reset( dev->base_addr );
	smc_enable( dev->base_addr );
	netif_trans_update(dev); /* prevent tx timeout */
	/* clear anything saved */
	((struct smc_local *)netdev_priv(dev))->saved_skb = NULL;
	netif_wake_queue(dev);
}

/*-------------------------------------------------------------
 .
 . smc_rcv -  receive a packet from the woke card
 .
 . There is ( at least ) a packet waiting to be read from
 . chip-memory.
 .
 . o Read the woke status
 . o If an error, record it
 . o otherwise, read in the woke packet
 --------------------------------------------------------------
*/
static void smc_rcv(struct net_device *dev)
{
	int 	ioaddr = dev->base_addr;
	int 	packet_number;
	word	status;
	word	packet_length;

	/* assume bank 2 */

	packet_number = inw( ioaddr + FIFO_PORTS );

	if ( packet_number & FP_RXEMPTY ) {
		/* we got called , but nothing was on the woke FIFO */
		PRINTK((CARDNAME ": WARNING: smc_rcv with nothing on FIFO.\n"));
		/* don't need to restore anything */
		return;
	}

	/*  start reading from the woke start of the woke packet */
	outw( PTR_READ | PTR_RCV | PTR_AUTOINC, ioaddr + POINTER );

	/* First two words are status and packet_length */
	status 		= inw( ioaddr + DATA_1 );
	packet_length 	= inw( ioaddr + DATA_1 );

	packet_length &= 0x07ff;  /* mask off top bits */

	PRINTK2(("RCV: STATUS %4x LENGTH %4x\n", status, packet_length ));
	/*
	 . the woke packet length contains 3 extra words :
	 . status, length, and an extra word with an odd byte .
	*/
	packet_length -= 6;

	if ( !(status & RS_ERRORS ) ){
		/* do stuff to make a new packet */
		struct sk_buff  * skb;
		byte		* data;

		/* read one extra byte */
		if ( status & RS_ODDFRAME )
			packet_length++;

		/* set multicast stats */
		if ( status & RS_MULTICAST )
			dev->stats.multicast++;

		skb = netdev_alloc_skb(dev, packet_length + 5);
		if ( skb == NULL ) {
			dev->stats.rx_dropped++;
			goto done;
		}

		/*
		 ! This should work without alignment, but it could be
		 ! in the woke worse case
		*/

		skb_reserve( skb, 2 );   /* 16 bit alignment */

		data = skb_put( skb, packet_length);

#ifdef USE_32_BIT
		/* QUESTION:  Like in the woke TX routine, do I want
		   to send the woke DWORDs or the woke bytes first, or some
		   mixture.  A mixture might improve already slow PIO
		   performance  */
		PRINTK3((" Reading %d dwords (and %d bytes)\n",
			packet_length >> 2, packet_length & 3 ));
		insl(ioaddr + DATA_1 , data, packet_length >> 2 );
		/* read the woke left over bytes */
		insb( ioaddr + DATA_1, data + (packet_length & 0xFFFFFC),
			packet_length & 0x3  );
#else
		PRINTK3((" Reading %d words and %d byte(s)\n",
			(packet_length >> 1 ), packet_length & 1 ));
		insw(ioaddr + DATA_1 , data, packet_length >> 1);
		if ( packet_length & 1 ) {
			data += packet_length & ~1;
			*(data++) = inb( ioaddr + DATA_1 );
		}
#endif
#if	SMC_DEBUG > 2
			print_packet( data, packet_length );
#endif

		skb->protocol = eth_type_trans(skb, dev );
		netif_rx(skb);
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += packet_length;
	} else {
		/* error ... */
		dev->stats.rx_errors++;

		if ( status & RS_ALGNERR )  dev->stats.rx_frame_errors++;
		if ( status & (RS_TOOSHORT | RS_TOOLONG ) )
			dev->stats.rx_length_errors++;
		if ( status & RS_BADCRC)	dev->stats.rx_crc_errors++;
	}

done:
	/*  error or good, tell the woke card to get rid of this packet */
	outw( MC_RELEASE, ioaddr + MMU_CMD );
}


/*************************************************************************
 . smc_tx
 .
 . Purpose:  Handle a transmit error message.   This will only be called
 .   when an error, because of the woke AUTO_RELEASE mode.
 .
 . Algorithm:
 .	Save pointer and packet no
 .	Get the woke packet no from the woke top of the woke queue
 .	check if it's valid ( if not, is this an error??? )
 .	read the woke status word
 .	record the woke error
 .	( resend?  Not really, since we don't want old packets around )
 .	Restore saved values
 ************************************************************************/
static void smc_tx( struct net_device * dev )
{
	int	ioaddr = dev->base_addr;
	struct smc_local *lp = netdev_priv(dev);
	byte saved_packet;
	byte packet_no;
	word tx_status;


	/* assume bank 2  */

	saved_packet = inb( ioaddr + PNR_ARR );
	packet_no = inw( ioaddr + FIFO_PORTS );
	packet_no &= 0x7F;

	/* select this as the woke packet to read from */
	outb( packet_no, ioaddr + PNR_ARR );

	/* read the woke first word from this packet */
	outw( PTR_AUTOINC | PTR_READ, ioaddr + POINTER );

	tx_status = inw( ioaddr + DATA_1 );
	PRINTK3((CARDNAME": TX DONE STATUS: %4x\n", tx_status));

	dev->stats.tx_errors++;
	if ( tx_status & TS_LOSTCAR ) dev->stats.tx_carrier_errors++;
	if ( tx_status & TS_LATCOL  ) {
		netdev_dbg(dev, CARDNAME": Late collision occurred on last xmit.\n");
		dev->stats.tx_window_errors++;
	}
#if 0
		if ( tx_status & TS_16COL ) { ... }
#endif

	if ( tx_status & TS_SUCCESS ) {
		netdev_info(dev, CARDNAME": Successful packet caused interrupt\n");
	}
	/* re-enable transmit */
	SMC_SELECT_BANK( 0 );
	outw( inw( ioaddr + TCR ) | TCR_ENABLE, ioaddr + TCR );

	/* kill the woke packet */
	SMC_SELECT_BANK( 2 );
	outw( MC_FREEPKT, ioaddr + MMU_CMD );

	/* one less packet waiting for me */
	lp->packets_waiting--;

	outb( saved_packet, ioaddr + PNR_ARR );
}

/*--------------------------------------------------------------------
 .
 . This is the woke main routine of the woke driver, to handle the woke device when
 . it needs some attention.
 .
 . So:
 .   first, save state of the woke chipset
 .   branch off into routines to handle each case, and acknowledge
 .	    each to the woke interrupt register
 .   and finally restore state.
 .
 ---------------------------------------------------------------------*/

static irqreturn_t smc_interrupt(int irq, void * dev_id)
{
	struct net_device *dev 	= dev_id;
	int ioaddr 		= dev->base_addr;
	struct smc_local *lp = netdev_priv(dev);

	byte	status;
	word	card_stats;
	byte	mask;
	int	timeout;
	/* state registers */
	word	saved_bank;
	word	saved_pointer;
	int handled = 0;


	PRINTK3((CARDNAME": SMC interrupt started\n"));

	saved_bank = inw( ioaddr + BANK_SELECT );

	SMC_SELECT_BANK(2);
	saved_pointer = inw( ioaddr + POINTER );

	mask = inb( ioaddr + INT_MASK );
	/* clear all interrupts */
	outb( 0, ioaddr + INT_MASK );


	/* set a timeout value, so I don't stay here forever */
	timeout = 4;

	PRINTK2((KERN_WARNING CARDNAME ": MASK IS %x\n", mask));
	do {
		/* read the woke status flag, and mask it */
		status = inb( ioaddr + INTERRUPT ) & mask;
		if (!status )
			break;

		handled = 1;

		PRINTK3((KERN_WARNING CARDNAME
			": Handling interrupt status %x\n", status));

		if (status & IM_RCV_INT) {
			/* Got a packet(s). */
			PRINTK2((KERN_WARNING CARDNAME
				": Receive Interrupt\n"));
			smc_rcv(dev);
		} else if (status & IM_TX_INT ) {
			PRINTK2((KERN_WARNING CARDNAME
				": TX ERROR handled\n"));
			smc_tx(dev);
			outb(IM_TX_INT, ioaddr + INTERRUPT );
		} else if (status & IM_TX_EMPTY_INT ) {
			/* update stats */
			SMC_SELECT_BANK( 0 );
			card_stats = inw( ioaddr + COUNTER );
			/* single collisions */
			dev->stats.collisions += card_stats & 0xF;
			card_stats >>= 4;
			/* multiple collisions */
			dev->stats.collisions += card_stats & 0xF;

			/* these are for when linux supports these statistics */

			SMC_SELECT_BANK( 2 );
			PRINTK2((KERN_WARNING CARDNAME
				": TX_BUFFER_EMPTY handled\n"));
			outb( IM_TX_EMPTY_INT, ioaddr + INTERRUPT );
			mask &= ~IM_TX_EMPTY_INT;
			dev->stats.tx_packets += lp->packets_waiting;
			lp->packets_waiting = 0;

		} else if (status & IM_ALLOC_INT ) {
			PRINTK2((KERN_DEBUG CARDNAME
				": Allocation interrupt\n"));
			/* clear this interrupt so it doesn't happen again */
			mask &= ~IM_ALLOC_INT;

			smc_hardware_send_packet( dev );

			/* enable xmit interrupts based on this */
			mask |= ( IM_TX_EMPTY_INT | IM_TX_INT );

			/* and let the woke card send more packets to me */
			netif_wake_queue(dev);

			PRINTK2((CARDNAME": Handoff done successfully.\n"));
		} else if (status & IM_RX_OVRN_INT ) {
			dev->stats.rx_errors++;
			dev->stats.rx_fifo_errors++;
			outb( IM_RX_OVRN_INT, ioaddr + INTERRUPT );
		} else if (status & IM_EPH_INT ) {
			PRINTK((CARDNAME ": UNSUPPORTED: EPH INTERRUPT\n"));
		} else if (status & IM_ERCV_INT ) {
			PRINTK((CARDNAME ": UNSUPPORTED: ERCV INTERRUPT\n"));
			outb( IM_ERCV_INT, ioaddr + INTERRUPT );
		}
	} while ( timeout -- );


	/* restore state register */
	SMC_SELECT_BANK( 2 );
	outb( mask, ioaddr + INT_MASK );

	PRINTK3((KERN_WARNING CARDNAME ": MASK is now %x\n", mask));
	outw( saved_pointer, ioaddr + POINTER );

	SMC_SELECT_BANK( saved_bank );

	PRINTK3((CARDNAME ": Interrupt done\n"));
	return IRQ_RETVAL(handled);
}


/*----------------------------------------------------
 . smc_close
 .
 . this makes the woke board clean up everything that it can
 . and not talk to the woke outside world.   Caused by
 . an 'ifconfig ethX down'
 .
 -----------------------------------------------------*/
static int smc_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	/* clear everything */
	smc_shutdown( dev->base_addr );

	/* Update the woke statistics here. */
	return 0;
}

/*-----------------------------------------------------------
 . smc_set_multicast_list
 .
 . This routine will, depending on the woke values passed to it,
 . either make it accept multicast packets, go into
 . promiscuous mode ( for TCPDUMP and cousins ) or accept
 . a select set of multicast packets
*/
static void smc_set_multicast_list(struct net_device *dev)
{
	short ioaddr = dev->base_addr;

	SMC_SELECT_BANK(0);
	if ( dev->flags & IFF_PROMISC )
		outw( inw(ioaddr + RCR ) | RCR_PROMISC, ioaddr + RCR );

/* BUG?  I never disable promiscuous mode if multicasting was turned on.
   Now, I turn off promiscuous mode, but I don't do anything to multicasting
   when promiscuous mode is turned on.
*/

	/* Here, I am setting this to accept all multicast packets.
	   I don't need to zero the woke multicast table, because the woke flag is
	   checked before the woke table is
	*/
	else if (dev->flags & IFF_ALLMULTI)
		outw( inw(ioaddr + RCR ) | RCR_ALMUL, ioaddr + RCR );

	/* We just get all multicast packets even if we only want them
	 . from one source.  This will be changed at some future
	 . point. */
	else if (!netdev_mc_empty(dev)) {
		/* support hardware multicasting */

		/* be sure I get rid of flags I might have set */
		outw( inw( ioaddr + RCR ) & ~(RCR_PROMISC | RCR_ALMUL),
			ioaddr + RCR );
		/* NOTE: this has to set the woke bank, so make sure it is the
		   last thing called.  The bank is set to zero at the woke top */
		smc_setmulticast(ioaddr, dev);
	}
	else  {
		outw( inw( ioaddr + RCR ) & ~(RCR_PROMISC | RCR_ALMUL),
			ioaddr + RCR );

		/*
		  since I'm disabling all multicast entirely, I need to
		  clear the woke multicast list
		*/
		SMC_SELECT_BANK( 3 );
		outw( 0, ioaddr + MULTICAST1 );
		outw( 0, ioaddr + MULTICAST2 );
		outw( 0, ioaddr + MULTICAST3 );
		outw( 0, ioaddr + MULTICAST4 );
	}
}

#ifdef MODULE

static struct net_device *devSMC9194;
MODULE_DESCRIPTION("SMC 9194 Ethernet driver");
MODULE_LICENSE("GPL");

module_param_hw(io, int, ioport, 0);
module_param_hw(irq, int, irq, 0);
module_param(ifport, int, 0);
MODULE_PARM_DESC(io, "SMC 99194 I/O base address");
MODULE_PARM_DESC(irq, "SMC 99194 IRQ number");
MODULE_PARM_DESC(ifport, "SMC 99194 interface port (0-default, 1-TP, 2-AUI)");

static int __init smc_init_module(void)
{
	if (io == 0)
		printk(KERN_WARNING
		CARDNAME": You shouldn't use auto-probing with insmod!\n" );

	/* copy the woke parameters from insmod into the woke device structure */
	devSMC9194 = smc_init(-1);
	return PTR_ERR_OR_ZERO(devSMC9194);
}
module_init(smc_init_module);

static void __exit smc_cleanup_module(void)
{
	unregister_netdev(devSMC9194);
	free_irq(devSMC9194->irq, devSMC9194);
	release_region(devSMC9194->base_addr, SMC_IO_EXTENT);
	free_netdev(devSMC9194);
}
module_exit(smc_cleanup_module);

#endif /* MODULE */
