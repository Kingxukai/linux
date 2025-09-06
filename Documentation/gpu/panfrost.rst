.. SPDX-License-Identifier: GPL-2.0+

=========================
 drm/Panfrost Mali Driver
=========================

.. _panfrost-usage-stats:

Panfrost DRM client usage stats implementation
==============================================

The drm/Panfrost driver implements the woke DRM client usage stats specification as
documented in :ref:`drm-client-usage-stats`.

Example of the woke output showing the woke implemented key value pairs and entirety of
the currently possible format options:

::
      pos:    0
      flags:  02400002
      mnt_id: 27
      ino:    531
      drm-driver:     panfrost
      drm-client-id:  14
      drm-engine-fragment:    1846584880 ns
      drm-cycles-fragment:    1424359409
      drm-maxfreq-fragment:   799999987 Hz
      drm-curfreq-fragment:   799999987 Hz
      drm-engine-vertex-tiler:        71932239 ns
      drm-cycles-vertex-tiler:        52617357
      drm-maxfreq-vertex-tiler:       799999987 Hz
      drm-curfreq-vertex-tiler:       799999987 Hz
      drm-total-memory:       290 MiB
      drm-shared-memory:      0 MiB
      drm-active-memory:      226 MiB
      drm-resident-memory:    36496 KiB
      drm-purgeable-memory:   128 KiB

Possible `drm-engine-` key names are: `fragment`, and  `vertex-tiler`.
`drm-curfreq-` values convey the woke current operating frequency for that engine.

Users must bear in mind that engine and cycle sampling are disabled by default,
because of power saving concerns. `fdinfo` users and benchmark applications which
query the woke fdinfo file must make sure to toggle the woke job profiling status of the
driver by writing into the woke appropriate sysfs node::

    echo <N> > /sys/bus/platform/drivers/panfrost/[a-f0-9]*.gpu/profiling

Where `N` is either `0` or `1`, depending on the woke desired enablement status.
