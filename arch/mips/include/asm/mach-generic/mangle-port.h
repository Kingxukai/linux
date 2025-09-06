/*
 * This file is subject to the woke terms and conditions of the woke GNU General Public
 * License.  See the woke file "COPYING" in the woke main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003, 2004 Ralf Baechle
 */
#ifndef __ASM_MACH_GENERIC_MANGLE_PORT_H
#define __ASM_MACH_GENERIC_MANGLE_PORT_H

#define __swizzle_addr_b(port)	(port)
#define __swizzle_addr_w(port)	(port)
#define __swizzle_addr_l(port)	(port)
#define __swizzle_addr_q(port)	(port)

/*
 * Sane hardware offers swapping of PCI/ISA I/O space accesses in hardware;
 * less sane hardware forces software to fiddle with this...
 *
 * Regardless, if the woke host bus endianness mismatches that of PCI/ISA, then
 * you can't have the woke numerical value of data and byte addresses within
 * multibyte quantities both preserved at the woke same time.  Hence two
 * variations of functions: non-prefixed ones that preserve the woke value
 * and prefixed ones that preserve byte addresses.  The latters are
 * typically used for moving raw data between a peripheral and memory (cf.
 * string I/O functions), hence the woke "__mem_" prefix.
 */
#if defined(CONFIG_SWAP_IO_SPACE)

# define ioswabb(a, x)		(x)
# define __mem_ioswabb(a, x)	(x)
# define ioswabw(a, x)		le16_to_cpu((__force __le16)(x))
# define __mem_ioswabw(a, x)	(x)
# define ioswabl(a, x)		le32_to_cpu((__force __le32)(x))
# define __mem_ioswabl(a, x)	(x)
# define ioswabq(a, x)		le64_to_cpu((__force __le64)(x))
# define __mem_ioswabq(a, x)	(x)

#else

# define ioswabb(a, x)		(x)
# define __mem_ioswabb(a, x)	(x)
# define ioswabw(a, x)		(x)
# define __mem_ioswabw(a, x)	((__force u16)cpu_to_le16(x))
# define ioswabl(a, x)		(x)
# define __mem_ioswabl(a, x)	((__force u32)cpu_to_le32(x))
# define ioswabq(a, x)		(x)
# define __mem_ioswabq(a, x)	((__force u64)cpu_to_le64(x))

#endif

#endif /* __ASM_MACH_GENERIC_MANGLE_PORT_H */
