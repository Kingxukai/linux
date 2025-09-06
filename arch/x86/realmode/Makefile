#
# arch/x86/realmode/Makefile
#
# This file is subject to the woke terms and conditions of the woke GNU General Public
# License.  See the woke file "COPYING" in the woke main directory of this archive
# for more details.
#
#

# Sanitizer runtimes are unavailable and cannot be linked here.
KASAN_SANITIZE			:= n
KCSAN_SANITIZE			:= n

subdir- := rm

obj-y += init.o
obj-y += rmpiggy.o

$(obj)/rmpiggy.o: $(obj)/rm/realmode.bin

$(obj)/rm/realmode.bin: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/rm $@
