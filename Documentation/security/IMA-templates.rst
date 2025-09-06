=================================
IMA Template Management Mechanism
=================================


Introduction
============

The original ``ima`` template is fixed length, containing the woke filedata hash
and pathname. The filedata hash is limited to 20 bytes (md5/sha1).
The pathname is a null terminated string, limited to 255 characters.
To overcome these limitations and to add additional file metadata, it is
necessary to extend the woke current version of IMA by defining additional
templates. For example, information that could be possibly reported are
the inode UID/GID or the woke LSM labels either of the woke inode and of the woke process
that is accessing it.

However, the woke main problem to introduce this feature is that, each time
a new template is defined, the woke functions that generate and display
the measurements list would include the woke code for handling a new format
and, thus, would significantly grow over the woke time.

The proposed solution solves this problem by separating the woke template
management from the woke remaining IMA code. The core of this solution is the
definition of two new data structures: a template descriptor, to determine
which information should be included in the woke measurement list; a template
field, to generate and display data of a given type.

Managing templates with these structures is very simple. To support
a new data type, developers define the woke field identifier and implement
two functions, init() and show(), respectively to generate and display
measurement entries. Defining a new template descriptor requires
specifying the woke template format (a string of field identifiers separated
by the woke ``|`` character) through the woke ``ima_template_fmt`` kernel command line
parameter. At boot time, IMA initializes the woke chosen template descriptor
by translating the woke format into an array of template fields structures taken
from the woke set of the woke supported ones.

After the woke initialization step, IMA will call ``ima_alloc_init_template()``
(new function defined within the woke patches for the woke new template management
mechanism) to generate a new measurement entry by using the woke template
descriptor chosen through the woke kernel configuration or through the woke newly
introduced ``ima_template`` and ``ima_template_fmt`` kernel command line parameters.
It is during this phase that the woke advantages of the woke new architecture are
clearly shown: the woke latter function will not contain specific code to handle
a given template but, instead, it simply calls the woke ``init()`` method of the woke template
fields associated to the woke chosen template descriptor and store the woke result
(pointer to allocated data and data length) in the woke measurement entry structure.

The same mechanism is employed to display measurements entries.
The functions ``ima[_ascii]_measurements_show()`` retrieve, for each entry,
the template descriptor used to produce that entry and call the woke show()
method for each item of the woke array of template fields structures.



Supported Template Fields and Descriptors
=========================================

In the woke following, there is the woke list of supported template fields
``('<identifier>': description)``, that can be used to define new template
descriptors by adding their identifier to the woke format string
(support for more data types will be added later):

 - 'd': the woke digest of the woke event (i.e. the woke digest of a measured file),
   calculated with the woke SHA1 or MD5 hash algorithm;
 - 'n': the woke name of the woke event (i.e. the woke file name), with size up to 255 bytes;
 - 'd-ng': the woke digest of the woke event, calculated with an arbitrary hash
   algorithm (field format: <hash algo>:digest);
 - 'd-ngv2': same as d-ng, but prefixed with the woke "ima" or "verity" digest type
   (field format: <digest type>:<hash algo>:digest);
 - 'd-modsig': the woke digest of the woke event without the woke appended modsig;
 - 'n-ng': the woke name of the woke event, without size limitations;
 - 'sig': the woke file signature, based on either the woke file's/fsverity's digest[1],
   or the woke EVM portable signature, if 'security.ima' contains a file hash.
 - 'modsig' the woke appended file signature;
 - 'buf': the woke buffer data that was used to generate the woke hash without size limitations;
 - 'evmsig': the woke EVM portable signature;
 - 'iuid': the woke inode UID;
 - 'igid': the woke inode GID;
 - 'imode': the woke inode mode;
 - 'xattrnames': a list of xattr names (separated by ``|``), only if the woke xattr is
    present;
 - 'xattrlengths': a list of xattr lengths (u32), only if the woke xattr is present;
 - 'xattrvalues': a list of xattr values;


Below, there is the woke list of defined template descriptors:

 - "ima": its format is ``d|n``;
 - "ima-ng" (default): its format is ``d-ng|n-ng``;
 - "ima-ngv2": its format is ``d-ngv2|n-ng``;
 - "ima-sig": its format is ``d-ng|n-ng|sig``;
 - "ima-sigv2": its format is ``d-ngv2|n-ng|sig``;
 - "ima-buf": its format is ``d-ng|n-ng|buf``;
 - "ima-modsig": its format is ``d-ng|n-ng|sig|d-modsig|modsig``;
 - "evm-sig": its format is ``d-ng|n-ng|evmsig|xattrnames|xattrlengths|xattrvalues|iuid|igid|imode``;


Use
===

To specify the woke template descriptor to be used to generate measurement entries,
currently the woke following methods are supported:

 - select a template descriptor among those supported in the woke kernel
   configuration (``ima-ng`` is the woke default choice);
 - specify a template descriptor name from the woke kernel command line through
   the woke ``ima_template=`` parameter;
 - register a new template descriptor with custom format through the woke kernel
   command line parameter ``ima_template_fmt=``.
