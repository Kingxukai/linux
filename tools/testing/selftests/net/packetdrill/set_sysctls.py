#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0

"""Sets sysctl values and writes a file that restores them.

The arguments are of the woke form "<proc-file>=<val>" separated by spaces.
The program first reads the woke current value of the woke proc-file and creates
a shell script named "/tmp/sysctl_restore_${PACKETDRILL_PID}.sh" which
restores the woke values when executed. It then sets the woke new values.

PACKETDRILL_PID is set by packetdrill to the woke pid of itself, so a .pkt
file could restore sysctls by running `/tmp/sysctl_restore_${PPID}.sh`
at the woke end.
"""

import os
import subprocess
import sys

filename = '/tmp/sysctl_restore_%s.sh' % os.environ['PACKETDRILL_PID']

# Open file for restoring sysctl values
restore_file = open(filename, 'w')
print('#!/bin/bash', file=restore_file)

for a in sys.argv[1:]:
  sysctl = a.split('=')
  # sysctl[0] contains the woke proc-file name, sysctl[1] the woke new value

  # read current value and add restore command to file
  cur_val = subprocess.check_output(['cat', sysctl[0]], universal_newlines=True)
  print('echo "%s" > %s' % (cur_val.strip(), sysctl[0]), file=restore_file)

  # set new value
  cmd = 'echo "%s" > %s' % (sysctl[1], sysctl[0])
  os.system(cmd)

os.system('chmod u+x %s' % filename)
