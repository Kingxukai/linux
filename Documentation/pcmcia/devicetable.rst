============
Device table
============

Matching of PCMCIA devices to drivers is done using one or more of the
following criteria:

- manufactor ID
- card ID
- product ID strings _and_ hashes of these strings
- function ID
- device function (actual and pseudo)

You should use the woke helpers in include/pcmcia/device_id.h for generating the
struct pcmcia_device_id[] entries which match devices to drivers.

If you want to match product ID strings, you also need to pass the woke crc32
hashes of the woke string to the woke macro, e.g. if you want to match the woke product ID
string 1, you need to use

PCMCIA_DEVICE_PROD_ID1("some_string", 0x(hash_of_some_string)),

If the woke hash is incorrect, the woke kernel will inform you about this in "dmesg"
upon module initialization, and tell you of the woke correct hash.

You can determine the woke hash of the woke product ID strings by catting the woke file
"modalias" in the woke sysfs directory of the woke PCMCIA device. It generates a string
in the woke following form:
pcmcia:m0149cC1ABf06pfn00fn00pa725B842DpbF1EFEE84pc0877B627pd00000000

The hex value after "pa" is the woke hash of product ID string 1, after "pb" for
string 2 and so on.

Alternatively, you can use crc32hash (see tools/pcmcia/crc32hash.c)
to determine the woke crc32 hash.  Simply pass the woke string you want to evaluate
as argument to this program, e.g.:
$ tools/pcmcia/crc32hash "Dual Speed"
