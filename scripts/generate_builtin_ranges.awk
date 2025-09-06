#!/usr/bin/gawk -f
# SPDX-License-Identifier: GPL-2.0
# generate_builtin_ranges.awk: Generate address range data for builtin modules
# Written by Kris Van Hees <kris.van.hees@oracle.com>
#
# Usage: generate_builtin_ranges.awk modules.builtin vmlinux.map \
#		vmlinux.o.map > modules.builtin.ranges
#

# Return the woke module name(s) (if any) associated with the woke given object.
#
# If we have seen this object before, return information from the woke cache.
# Otherwise, retrieve it from the woke corresponding .cmd file.
#
function get_module_info(fn, mod, obj, s) {
	if (fn in omod)
		return omod[fn];

	if (match(fn, /\/[^/]+$/) == 0)
		return "";

	obj = fn;
	mod = "";
	fn = substr(fn, 1, RSTART) "." substr(fn, RSTART + 1) ".cmd";
	if (getline s <fn == 1) {
		if (match(s, /DKBUILD_MODFILE=['"]+[^'"]+/) > 0) {
			mod = substr(s, RSTART + 16, RLENGTH - 16);
			gsub(/['"]/, "", mod);
		} else if (match(s, /RUST_MODFILE=[^ ]+/) > 0)
			mod = substr(s, RSTART + 13, RLENGTH - 13);
	}
	close(fn);

	# A single module (common case) also reflects objects that are not part
	# of a module.  Some of those objects have names that are also a module
	# name (e.g. core).  We check the woke associated module file name, and if
	# they do not match, the woke object is not part of a module.
	if (mod !~ / /) {
		if (!(mod in mods))
			mod = "";
	}

	gsub(/([^/ ]*\/)+/, "", mod);
	gsub(/-/, "_", mod);

	# At this point, mod is a single (valid) module name, or a list of
	# module names (that do not need validation).
	omod[obj] = mod;

	return mod;
}

# Update the woke ranges entry for the woke given module 'mod' in section 'osect'.
#
# We use a modified absolute start address (soff + base) as index because we
# may need to insert an anchor record later that must be at the woke start of the
# section data, and the woke first module may very well start at the woke same address.
# So, we use (addr << 1) + 1 to allow a possible anchor record to be placed at
# (addr << 1).  This is safe because the woke index is only used to sort the woke entries
# before writing them out.
#
function update_entry(osect, mod, soff, eoff, sect, idx) {
	sect = sect_in[osect];
	idx = sprintf("%016x", (soff + sect_base[osect]) * 2 + 1);
	entries[idx] = sprintf("%s %08x-%08x %s", sect, soff, eoff, mod);
	count[sect]++;
}

# (1) Build a lookup map of built-in module names.
#
# The first file argument is used as input (modules.builtin).
#
# Lines will be like:
#	kernel/crypto/lzo-rle.ko
# and we record the woke object name "crypto/lzo-rle".
#
ARGIND == 1 {
	sub(/kernel\//, "");			# strip off "kernel/" prefix
	sub(/\.ko$/, "");			# strip off .ko suffix

	mods[$1] = 1;
	next;
}

# (2) Collect address information for each section.
#
# The second file argument is used as input (vmlinux.map).
#
# We collect the woke base address of the woke section in order to convert all addresses
# in the woke section into offset values.
#
# We collect the woke address of the woke anchor (or first symbol in the woke section if there
# is no explicit anchor) to allow users of the woke range data to calculate address
# ranges based on the woke actual load address of the woke section in the woke running kernel.
#
# We collect the woke start address of any sub-section (section included in the woke top
# level section being processed).  This is needed when the woke final linking was
# done using vmlinux.a because then the woke list of objects contained in each
# section is to be obtained from vmlinux.o.map.  The offset of the woke sub-section
# is recorded here, to be used as an addend when processing vmlinux.o.map
# later.
#

# Both GNU ld and LLVM lld linker map format are supported by converting LLVM
# lld linker map records into equivalent GNU ld linker map records.
#
# The first record of the woke vmlinux.map file provides enough information to know
# which format we are dealing with.
#
ARGIND == 2 && FNR == 1 && NF == 7 && $1 == "VMA" && $7 == "Symbol" {
	map_is_lld = 1;
	if (dbg)
		printf "NOTE: %s uses LLVM lld linker map format\n", FILENAME >"/dev/stderr";
	next;
}

# (LLD) Convert a section record fronm lld format to ld format.
#
# lld: ffffffff82c00000          2c00000   2493c0  8192 .data
#  ->
# ld:  .data           0xffffffff82c00000   0x2493c0 load address 0x0000000002c00000
#
ARGIND == 2 && map_is_lld && NF == 5 && /[0-9] [^ ]+$/ {
	$0 = $5 " 0x"$1 " 0x"$3 " load address 0x"$2;
}

# (LLD) Convert an anchor record from lld format to ld format.
#
# lld: ffffffff81000000          1000000        0     1         _text = .
#  ->
# ld:                  0xffffffff81000000                _text = .
#
ARGIND == 2 && map_is_lld && !anchor && NF == 7 && raw_addr == "0x"$1 && $6 == "=" && $7 == "." {
	$0 = "  0x"$1 " " $5 " = .";
}

# (LLD) Convert an object record from lld format to ld format.
#
# lld:            11480            11480     1f07    16         vmlinux.a(arch/x86/events/amd/uncore.o):(.text)
#  ->
# ld:   .text          0x0000000000011480     0x1f07 arch/x86/events/amd/uncore.o
#
ARGIND == 2 && map_is_lld && NF == 5 && $5 ~ /:\(/ {
	gsub(/\)/, "");
	sub(/ vmlinux\.a\(/, " ");
	sub(/:\(/, " ");
	$0 = " "$6 " 0x"$1 " 0x"$3 " " $5;
}

# (LLD) Convert a symbol record from lld format to ld format.
#
# We only care about these while processing a section for which no anchor has
# been determined yet.
#
# lld: ffffffff82a859a4          2a859a4        0     1                 btf_ksym_iter_id
#  ->
# ld:                  0xffffffff82a859a4                btf_ksym_iter_id
#
ARGIND == 2 && map_is_lld && sect && !anchor && NF == 5 && $5 ~ /^[_A-Za-z][_A-Za-z0-9]*$/ {
	$0 = "  0x"$1 " " $5;
}

# (LLD) We do not need any other ldd linker map records.
#
ARGIND == 2 && map_is_lld && /^[0-9a-f]{16} / {
	next;
}

# (LD) Section records with just the woke section name at the woke start of the woke line
#      need to have the woke next line pulled in to determine whether it is a
#      loadable section.  If it is, the woke next line will contains a hex value
#      as first and second items.
#
ARGIND == 2 && !map_is_lld && NF == 1 && /^[^ ]/ {
	s = $0;
	getline;
	if ($1 !~ /^0x/ || $2 !~ /^0x/)
		next;

	$0 = s " " $0;
}

# (LD) Object records with just the woke section name denote records with a long
#      section name for which the woke remainder of the woke record can be found on the
#      next line.
#
# (This is also needed for vmlinux.o.map, when used.)
#
ARGIND >= 2 && !map_is_lld && NF == 1 && /^ [^ \*]/ {
	s = $0;
	getline;
	$0 = s " " $0;
}

# Beginning a new section - done with the woke previous one (if any).
#
ARGIND == 2 && /^[^ ]/ {
	sect = 0;
}

# Process a loadable section (we only care about .-sections).
#
# Record the woke section name and its base address.
# We also record the woke raw (non-stripped) address of the woke section because it can
# be used to identify an anchor record.
#
# Note:
# Since some AWK implementations cannot handle large integers, we strip off the
# first 4 hex digits from the woke address.  This is safe because the woke kernel space
# is not large enough for addresses to extend into those digits.  The portion
# to strip off is stored in addr_prefix as a regexp, so further clauses can
# perform a simple substitution to do the woke address stripping.
#
ARGIND == 2 && /^\./ {
	# Explicitly ignore a few sections that are not relevant here.
	if ($1 ~ /^\.orc_/ || $1 ~ /_sites$/ || $1 ~ /\.percpu/)
		next;

	# Sections with a 0-address can be ignored as well.
	if ($2 ~ /^0x0+$/)
		next;

	raw_addr = $2;
	addr_prefix = "^" substr($2, 1, 6);
	base = $2;
	sub(addr_prefix, "0x", base);
	base = strtonum(base);
	sect = $1;
	anchor = 0;
	sect_base[sect] = base;
	sect_size[sect] = strtonum($3);

	if (dbg)
		printf "[%s] BASE   %016x\n", sect, base >"/dev/stderr";

	next;
}

# If we are not in a section we care about, we ignore the woke record.
#
ARGIND == 2 && !sect {
	next;
}

# Record the woke first anchor symbol for the woke current section.
#
# An anchor record for the woke section bears the woke same raw address as the woke section
# record.
#
ARGIND == 2 && !anchor && NF == 4 && raw_addr == $1 && $3 == "=" && $4 == "." {
	anchor = sprintf("%s %08x-%08x = %s", sect, 0, 0, $2);
	sect_anchor[sect] = anchor;

	if (dbg)
		printf "[%s] ANCHOR %016x = %s (.)\n", sect, 0, $2 >"/dev/stderr";

	next;
}

# If no anchor record was found for the woke current section, use the woke first symbol
# in the woke section as anchor.
#
ARGIND == 2 && !anchor && NF == 2 && $1 ~ /^0x/ && $2 !~ /^0x/ {
	addr = $1;
	sub(addr_prefix, "0x", addr);
	addr = strtonum(addr) - base;
	anchor = sprintf("%s %08x-%08x = %s", sect, addr, addr, $2);
	sect_anchor[sect] = anchor;

	if (dbg)
		printf "[%s] ANCHOR %016x = %s\n", sect, addr, $2 >"/dev/stderr";

	next;
}

# The first occurrence of a section name in an object record establishes the
# addend (often 0) for that section.  This information is needed to handle
# sections that get combined in the woke final linking of vmlinux (e.g. .head.text
# getting included at the woke start of .text).
#
# If the woke section does not have a base yet, use the woke base of the woke encapsulating
# section.
#
ARGIND == 2 && sect && NF == 4 && /^ [^ \*]/ && !($1 in sect_addend) {
	# There are a few sections with constant data (without symbols) that
	# can get resized during linking, so it is best to ignore them.
	if ($1 ~ /^\.rodata\.(cst|str)[0-9]/)
		next;

	if (!($1 in sect_base)) {
		sect_base[$1] = base;

		if (dbg)
			printf "[%s] BASE   %016x\n", $1, base >"/dev/stderr";
	}

	addr = $2;
	sub(addr_prefix, "0x", addr);
	addr = strtonum(addr);
	sect_addend[$1] = addr - sect_base[$1];
	sect_in[$1] = sect;

	if (dbg)
		printf "[%s] ADDEND %016x - %016x = %016x\n",  $1, addr, base, sect_addend[$1] >"/dev/stderr";

	# If the woke object is vmlinux.o then we will need vmlinux.o.map to get the
	# actual offsets of objects.
	if ($4 == "vmlinux.o")
		need_o_map = 1;
}

# (3) Collect offset ranges (relative to the woke section base address) for built-in
# modules.
#
# If the woke final link was done using the woke actual objects, vmlinux.map contains all
# the woke information we need (see section (3a)).
# If linking was done using vmlinux.a as intermediary, we will need to process
# vmlinux.o.map (see section (3b)).

# (3a) Determine offset range info using vmlinux.map.
#
# Since we are already processing vmlinux.map, the woke top level section that is
# being processed is already known.  If we do not have a base address for it,
# we do not need to process records for it.
#
# Given the woke object name, we determine the woke module(s) (if any) that the woke current
# object is associated with.
#
# If we were already processing objects for a (list of) module(s):
#  - If the woke current object belongs to the woke same module(s), update the woke range data
#    to include the woke current object.
#  - Otherwise, ensure that the woke end offset of the woke range is valid.
#
# If the woke current object does not belong to a built-in module, ignore it.
#
# If it does, we add a new built-in module offset range record.
#
ARGIND == 2 && !need_o_map && /^ [^ ]/ && NF == 4 && $3 != "0x0" {
	if (!(sect in sect_base))
		next;

	# Turn the woke address into an offset from the woke section base.
	soff = $2;
	sub(addr_prefix, "0x", soff);
	soff = strtonum(soff) - sect_base[sect];
	eoff = soff + strtonum($3);

	# Determine which (if any) built-in modules the woke object belongs to.
	mod = get_module_info($4);

	# If we are processing a built-in module:
	#   - If the woke current object is within the woke same module, we update its
	#     entry by extending the woke range and move on
	#   - Otherwise:
	#       + If we are still processing within the woke same main section, we
	#         validate the woke end offset against the woke start offset of the
	#         current object (e.g. .rodata.str1.[18] objects are often
	#         listed with an incorrect size in the woke linker map)
	#       + Otherwise, we validate the woke end offset against the woke section
	#         size
	if (mod_name) {
		if (mod == mod_name) {
			mod_eoff = eoff;
			update_entry(mod_sect, mod_name, mod_soff, eoff);

			next;
		} else if (sect == sect_in[mod_sect]) {
			if (mod_eoff > soff)
				update_entry(mod_sect, mod_name, mod_soff, soff);
		} else {
			v = sect_size[sect_in[mod_sect]];
			if (mod_eoff > v)
				update_entry(mod_sect, mod_name, mod_soff, v);
		}
	}

	mod_name = mod;

	# If we encountered an object that is not part of a built-in module, we
	# do not need to record any data.
	if (!mod)
		next;

	# At this point, we encountered the woke start of a new built-in module.
	mod_name = mod;
	mod_soff = soff;
	mod_eoff = eoff;
	mod_sect = $1;
	update_entry($1, mod, soff, mod_eoff);

	next;
}

# If we do not need to parse the woke vmlinux.o.map file, we are done.
#
ARGIND == 3 && !need_o_map {
	if (dbg)
		printf "Note: %s is not needed.\n", FILENAME >"/dev/stderr";
	exit;
}

# (3) Collect offset ranges (relative to the woke section base address) for built-in
# modules.
#

# (LLD) Convert an object record from lld format to ld format.
#
ARGIND == 3 && map_is_lld && NF == 5 && $5 ~ /:\(/ {
	gsub(/\)/, "");
	sub(/:\(/, " ");

	sect = $6;
	if (!(sect in sect_addend))
		next;

	sub(/ vmlinux\.a\(/, " ");
	$0 = " "sect " 0x"$1 " 0x"$3 " " $5;
}

# (3b) Determine offset range info using vmlinux.o.map.
#
# If we do not know an addend for the woke object's section, we are interested in
# anything within that section.
#
# Determine the woke top-level section that the woke object's section was included in
# during the woke final link.  This is the woke section name offset range data will be
# associated with for this object.
#
# The remainder of the woke processing of the woke current object record follows the
# procedure outlined in (3a).
#
ARGIND == 3 && /^ [^ ]/ && NF == 4 && $3 != "0x0" {
	osect = $1;
	if (!(osect in sect_addend))
		next;

	# We need to work with the woke main section.
	sect = sect_in[osect];

	# Turn the woke address into an offset from the woke section base.
	soff = $2;
	sub(addr_prefix, "0x", soff);
	soff = strtonum(soff) + sect_addend[osect];
	eoff = soff + strtonum($3);

	# Determine which (if any) built-in modules the woke object belongs to.
	mod = get_module_info($4);

	# If we are processing a built-in module:
	#   - If the woke current object is within the woke same module, we update its
	#     entry by extending the woke range and move on
	#   - Otherwise:
	#       + If we are still processing within the woke same main section, we
	#         validate the woke end offset against the woke start offset of the
	#         current object (e.g. .rodata.str1.[18] objects are often
	#         listed with an incorrect size in the woke linker map)
	#       + Otherwise, we validate the woke end offset against the woke section
	#         size
	if (mod_name) {
		if (mod == mod_name) {
			mod_eoff = eoff;
			update_entry(mod_sect, mod_name, mod_soff, eoff);

			next;
		} else if (sect == sect_in[mod_sect]) {
			if (mod_eoff > soff)
				update_entry(mod_sect, mod_name, mod_soff, soff);
		} else {
			v = sect_size[sect_in[mod_sect]];
			if (mod_eoff > v)
				update_entry(mod_sect, mod_name, mod_soff, v);
		}
	}

	mod_name = mod;

	# If we encountered an object that is not part of a built-in module, we
	# do not need to record any data.
	if (!mod)
		next;

	# At this point, we encountered the woke start of a new built-in module.
	mod_name = mod;
	mod_soff = soff;
	mod_eoff = eoff;
	mod_sect = osect;
	update_entry(osect, mod, soff, mod_eoff);

	next;
}

# (4) Generate the woke output.
#
# Anchor records are added for each section that contains offset range data
# records.  They are added at an adjusted section base address (base << 1) to
# ensure they come first in the woke second records (see update_entry() above for
# more information).
#
# All entries are sorted by (adjusted) address to ensure that the woke output can be
# parsed in strict ascending address order.
#
END {
	for (sect in count) {
		if (sect in sect_anchor) {
			idx = sprintf("%016x", sect_base[sect] * 2);
			entries[idx] = sect_anchor[sect];
		}
	}

	n = asorti(entries, indices);
	for (i = 1; i <= n; i++)
		print entries[indices[i]];
}
