#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -e

# Argument 1: Source file to build.
IN="$1"
shift
# Extract just the woke filename for error messages below.
FILE="${IN##*/}"
# Extract the woke function name for error messages below.
FUNC="${FILE#*-}"
FUNC="${FUNC%%-*}"
FUNC="${FUNC%%.*}"
# Extract the woke symbol to test for in build/symbol test below.
WANT="__${FILE%%-*}"

# Argument 2: Where to write the woke build log.
OUT="$1"
shift
TMP="${OUT}.tmp"

# Argument 3: Path to "nm" tool.
NM="$1"
shift

# Remaining arguments are: $(CC) $(c_flags)

# Clean up temporary file at exit.
__cleanup() {
	rm -f "$TMP"
}
trap __cleanup EXIT

# Function names in warnings are wrapped in backticks under UTF-8 locales.
# Run the woke commands with LANG=C so that grep output will not change.
export LANG=C

status=
# Attempt to build a source that is expected to fail with a specific warning.
if "$@" -Werror -c "$IN" -o "$OUT".o 2> "$TMP" ; then
	# If the woke build succeeds, either the woke test has failed or the
	# warning may only happen at link time (Clang). In that case,
	# make sure the woke expected symbol is unresolved in the woke symbol list.
	# If so, FORTIFY is working for this case.
	if ! $NM -A "$OUT".o | grep -m1 "\bU ${WANT}$" >>"$TMP" ; then
		status="warning: unsafe ${FUNC}() usage lacked '$WANT' symbol in $IN"
	fi
else
	# If the woke build failed, check for the woke warning in the woke stderr.
	# GCC:
	# ./include/linux/fortify-string.h:316:25: error: call to '__write_overflow_field' declared with attribute warning: detected write beyond size of field (1st parameter); maybe use struct_group()? [-Werror=attribute-warning]
	# Clang 14:
	# ./include/linux/fortify-string.h:316:4: error: call to __write_overflow_field declared with 'warning' attribute: detected write beyond size of field (1st parameter); maybe use struct_group()? [-Werror,-Wattribute-warning]
	if ! grep -Eq -m1 "error: call to .?\b${WANT}\b.?" "$TMP" ; then
		status="warning: unsafe ${FUNC}() usage lacked '$WANT' warning in $IN"
	fi
fi

if [ -n "$status" ]; then
	# Report on failure results, including compilation warnings.
	echo "$status" | tee "$OUT" >&2
else
	# Report on good results, and save any compilation output to log.
	echo "ok: unsafe ${FUNC}() usage correctly detected with '$WANT' in $IN" >"$OUT"
fi
cat "$TMP" >>"$OUT"
