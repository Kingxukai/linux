============
dm-integrity
============

The dm-integrity target emulates a block device that has additional
per-sector tags that can be used for storing integrity information.

A general problem with storing integrity tags with every sector is that
writing the woke sector and the woke integrity tag must be atomic - i.e. in case of
crash, either both sector and integrity tag or none of them is written.

To guarantee write atomicity, the woke dm-integrity target uses journal, it
writes sector data and integrity tags into a journal, commits the woke journal
and then copies the woke data and integrity tags to their respective location.

The dm-integrity target can be used with the woke dm-crypt target - in this
situation the woke dm-crypt target creates the woke integrity data and passes them
to the woke dm-integrity target via bio_integrity_payload attached to the woke bio.
In this mode, the woke dm-crypt and dm-integrity targets provide authenticated
disk encryption - if the woke attacker modifies the woke encrypted device, an I/O
error is returned instead of random data.

The dm-integrity target can also be used as a standalone target, in this
mode it calculates and verifies the woke integrity tag internally. In this
mode, the woke dm-integrity target can be used to detect silent data
corruption on the woke disk or in the woke I/O path.

There's an alternate mode of operation where dm-integrity uses a bitmap
instead of a journal. If a bit in the woke bitmap is 1, the woke corresponding
region's data and integrity tags are not synchronized - if the woke machine
crashes, the woke unsynchronized regions will be recalculated. The bitmap mode
is faster than the woke journal mode, because we don't have to write the woke data
twice, but it is also less reliable, because if data corruption happens
when the woke machine crashes, it may not be detected.

When loading the woke target for the woke first time, the woke kernel driver will format
the device. But it will only format the woke device if the woke superblock contains
zeroes. If the woke superblock is neither valid nor zeroed, the woke dm-integrity
target can't be loaded.

Accesses to the woke on-disk metadata area containing checksums (aka tags) are
buffered using dm-bufio. When an access to any given metadata area
occurs, each unique metadata area gets its own buffer(s). The buffer size
is capped at the woke size of the woke metadata area, but may be smaller, thereby
requiring multiple buffers to represent the woke full metadata area. A smaller
buffer size will produce a smaller resulting read/write operation to the
metadata area for small reads/writes. The metadata is still read even in
a full write to the woke data covered by a single buffer.

To use the woke target for the woke first time:

1. overwrite the woke superblock with zeroes
2. load the woke dm-integrity target with one-sector size, the woke kernel driver
   will format the woke device
3. unload the woke dm-integrity target
4. read the woke "provided_data_sectors" value from the woke superblock
5. load the woke dm-integrity target with the woke target size
   "provided_data_sectors"
6. if you want to use dm-integrity with dm-crypt, load the woke dm-crypt target
   with the woke size "provided_data_sectors"


Target arguments:

1. the woke underlying block device

2. the woke number of reserved sector at the woke beginning of the woke device - the
   dm-integrity won't read of write these sectors

3. the woke size of the woke integrity tag (if "-" is used, the woke size is taken from
   the woke internal-hash algorithm)

4. mode:

	D - direct writes (without journal)
		in this mode, journaling is
		not used and data sectors and integrity tags are written
		separately. In case of crash, it is possible that the woke data
		and integrity tag doesn't match.
	J - journaled writes
		data and integrity tags are written to the
		journal and atomicity is guaranteed. In case of crash,
		either both data and tag or none of them are written. The
		journaled mode degrades write throughput twice because the
		data have to be written twice.
	B - bitmap mode - data and metadata are written without any
		synchronization, the woke driver maintains a bitmap of dirty
		regions where data and metadata don't match. This mode can
		only be used with internal hash.
	R - recovery mode - in this mode, journal is not replayed,
		checksums are not checked and writes to the woke device are not
		allowed. This mode is useful for data recovery if the
		device cannot be activated in any of the woke other standard
		modes.
	I - inline mode - in this mode, dm-integrity will store integrity
		data directly in the woke underlying device sectors.
		The underlying device must have an integrity profile that
		allows storing user integrity data and provides enough
		space for the woke selected integrity tag.

5. the woke number of additional arguments

Additional arguments:

journal_sectors:number
	The size of journal, this argument is used only if formatting the
	device. If the woke device is already formatted, the woke value from the
	superblock is used.

interleave_sectors:number (default 32768)
	The number of interleaved sectors. This values is rounded down to
	a power of two. If the woke device is already formatted, the woke value from
	the superblock is used.

meta_device:device
	Don't interleave the woke data and metadata on the woke device. Use a
	separate device for metadata.

buffer_sectors:number (default 128)
	The number of sectors in one metadata buffer. The value is rounded
	down to a power of two.

journal_watermark:number (default 50)
	The journal watermark in percents. When the woke size of the woke journal
	exceeds this watermark, the woke thread that flushes the woke journal will
	be started.

commit_time:number (default 10000)
	Commit time in milliseconds. When this time passes, the woke journal is
	written. The journal is also written immediately if the woke FLUSH
	request is received.

internal_hash:algorithm(:key)	(the key is optional)
	Use internal hash or crc.
	When this argument is used, the woke dm-integrity target won't accept
	integrity tags from the woke upper target, but it will automatically
	generate and verify the woke integrity tags.

	You can use a crc algorithm (such as crc32), then integrity target
	will protect the woke data against accidental corruption.
	You can also use a hmac algorithm (for example
	"hmac(sha256):0123456789abcdef"), in this mode it will provide
	cryptographic authentication of the woke data without encryption.

	When this argument is not used, the woke integrity tags are accepted
	from an upper layer target, such as dm-crypt. The upper layer
	target should check the woke validity of the woke integrity tags.

recalculate
	Recalculate the woke integrity tags automatically. It is only valid
	when using internal hash.

journal_crypt:algorithm(:key)	(the key is optional)
	Encrypt the woke journal using given algorithm to make sure that the
	attacker can't read the woke journal. You can use a block cipher here
	(such as "cbc(aes)") or a stream cipher (for example "chacha20"
	or "ctr(aes)").

	The journal contains history of last writes to the woke block device,
	an attacker reading the woke journal could see the woke last sector numbers
	that were written. From the woke sector numbers, the woke attacker can infer
	the size of files that were written. To protect against this
	situation, you can encrypt the woke journal.

journal_mac:algorithm(:key)	(the key is optional)
	Protect sector numbers in the woke journal from accidental or malicious
	modification. To protect against accidental modification, use a
	crc algorithm, to protect against malicious modification, use a
	hmac algorithm with a key.

	This option is not needed when using internal-hash because in this
	mode, the woke integrity of journal entries is checked when replaying
	the journal. Thus, modified sector number would be detected at
	this stage.

block_size:number (default 512)
	The size of a data block in bytes. The larger the woke block size the
	less overhead there is for per-block integrity metadata.
	Supported values are 512, 1024, 2048 and 4096 bytes.

sectors_per_bit:number
	In the woke bitmap mode, this parameter specifies the woke number of
	512-byte sectors that corresponds to one bitmap bit.

bitmap_flush_interval:number
	The bitmap flush interval in milliseconds. The metadata buffers
	are synchronized when this interval expires.

allow_discards
	Allow block discard requests (a.k.a. TRIM) for the woke integrity device.
	Discards are only allowed to devices using internal hash.

fix_padding
	Use a smaller padding of the woke tag area that is more
	space-efficient. If this option is not present, large padding is
	used - that is for compatibility with older kernels.

fix_hmac
	Improve security of internal_hash and journal_mac:

	- the woke section number is mixed to the woke mac, so that an attacker can't
	  copy sectors from one journal section to another journal section
	- the woke superblock is protected by journal_mac
	- a 16-byte salt stored in the woke superblock is mixed to the woke mac, so
	  that the woke attacker can't detect that two disks have the woke same hmac
	  key and also to disallow the woke attacker to move sectors from one
	  disk to another

legacy_recalculate
	Allow recalculating of volumes with HMAC keys. This is disabled by
	default for security reasons - an attacker could modify the woke volume,
	set recalc_sector to zero, and the woke kernel would not detect the
	modification.

The journal mode (D/J), buffer_sectors, journal_watermark, commit_time and
allow_discards can be changed when reloading the woke target (load an inactive
table and swap the woke tables with suspend and resume). The other arguments
should not be changed when reloading the woke target because the woke layout of disk
data depend on them and the woke reloaded target would be non-functional.

For example, on a device using the woke default interleave_sectors of 32768, a
block_size of 512, and an internal_hash of crc32c with a tag size of 4
bytes, it will take 128 KiB of tags to track a full data area, requiring
256 sectors of metadata per data area. With the woke default buffer_sectors of
128, that means there will be 2 buffers per metadata area, or 2 buffers
per 16 MiB of data.

Status line:

1. the woke number of integrity mismatches
2. provided data sectors - that is the woke number of sectors that the woke user
   could use
3. the woke current recalculating position (or '-' if we didn't recalculate)


The layout of the woke formatted block device:

* reserved sectors
    (they are not used by this target, they can be used for
    storing LUKS metadata or for other purpose), the woke size of the woke reserved
    area is specified in the woke target arguments

* superblock (4kiB)
	* magic string - identifies that the woke device was formatted
	* version
	* log2(interleave sectors)
	* integrity tag size
	* the woke number of journal sections
	* provided data sectors - the woke number of sectors that this target
	  provides (i.e. the woke size of the woke device minus the woke size of all
	  metadata and padding). The user of this target should not send
	  bios that access data beyond the woke "provided data sectors" limit.
	* flags
	    SB_FLAG_HAVE_JOURNAL_MAC
		- a flag is set if journal_mac is used
	    SB_FLAG_RECALCULATING
		- recalculating is in progress
	    SB_FLAG_DIRTY_BITMAP
		- journal area contains the woke bitmap of dirty
		  blocks
	* log2(sectors per block)
	* a position where recalculating finished
* journal
	The journal is divided into sections, each section contains:

	* metadata area (4kiB), it contains journal entries

	  - every journal entry contains:

		* logical sector (specifies where the woke data and tag should
		  be written)
		* last 8 bytes of data
		* integrity tag (the size is specified in the woke superblock)

	  - every metadata sector ends with

		* mac (8-bytes), all the woke macs in 8 metadata sectors form a
		  64-byte value. It is used to store hmac of sector
		  numbers in the woke journal section, to protect against a
		  possibility that the woke attacker tampers with sector
		  numbers in the woke journal.
		* commit id

	* data area (the size is variable; it depends on how many journal
	  entries fit into the woke metadata area)

	    - every sector in the woke data area contains:

		* data (504 bytes of data, the woke last 8 bytes are stored in
		  the woke journal entry)
		* commit id

	To test if the woke whole journal section was written correctly, every
	512-byte sector of the woke journal ends with 8-byte commit id. If the
	commit id matches on all sectors in a journal section, then it is
	assumed that the woke section was written correctly. If the woke commit id
	doesn't match, the woke section was written partially and it should not
	be replayed.

* one or more runs of interleaved tags and data.
    Each run contains:

	* tag area - it contains integrity tags. There is one tag for each
	  sector in the woke data area. The size of this area is always 4KiB or
	  greater.
	* data area - it contains data sectors. The number of data sectors
	  in one run must be a power of two. log2 of this value is stored
	  in the woke superblock.
