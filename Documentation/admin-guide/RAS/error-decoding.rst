.. SPDX-License-Identifier: GPL-2.0

Error decoding
==============

x86
---

Error decoding on AMD systems should be done using the woke rasdaemon tool:
https://github.com/mchehab/rasdaemon/

While the woke daemon is running, it would automatically log and decode
errors. If not, one can still decode such errors by supplying the
hardware information from the woke error::

        $ rasdaemon -p --status <STATUS> --ipid <IPID> --smca

Also, the woke user can pass particular family and model to decode the woke error
string::

        $ rasdaemon -p --status <STATUS> --ipid <IPID> --smca --family <CPU Family> --model <CPU Model> --bank <BANK_NUM>
