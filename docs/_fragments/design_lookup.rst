.. _dyad_metadata_lookup:

File Lookup and Hierarchical File Discovery
###########################################

DYAD employs a multi-level Hierarchical File Locator (HFL) to efficiently
locate files across distributed nodes without creating metadata bottlenecks
when multiple I/O workers access files concurrently. The locator searches
metadata through three levels in order of increasing scope and latency:
the local DYAD Managed Directory (DMD), the node-local cache of the HFL
key-value store, and the remaining levels of the HFL hierarchy, which
distributes the search across nodes via a hierarchical key-value search
tree operated by underlying `Flux KVS <https://flux-framework.readthedocs.io/projects/flux-core/en/latest/guide/kvs.html#key-value-store>`_.
Workers always check local sources first, reducing redundant network
searches and balancing workload across the cluster. As files are located
via metadata lookups, their metadata is cached at each level so that future
lookups resolve at the lowest possible level. The local DMD uses filesystem
metadata for the fastest lookups, i.e., obtaining a file lock on a file
descriptor; the local HFL is approximately one order of magnitude slower
than the DMD but one order of magnitude faster than the global HFL,
providing an efficient intermediate cache for node-local misses.

.. figure:: _static/figs/hierarchical-file-discovery.svg
   :alt: Hierarchical File Locator — four cases

   Four cases illustrating the hierarchical file locator. In Case 1, both
   worker 1 (W1) and worker 2 (W2) are on the same node and the file is
   already in the local DMD — W2 finds it immediately without any network
   lookup because W1 had already cached it there (i.e., written it to the
   local DMD). In Case 2, the file was evicted from the DMD before W2's
   search, so W2 falls back to the local HFL to retrieve the metadata of
   the file. In Case 3, W1 and W2 are on different nodes. W1 had already
   cached the files locally (i.e., produced the files into its local DMD),
   and thus W2 finds the file in the global HFL due to a miss in its local
   caches. In Case 4, no cache contains the file and the locator falls
   back to the parallel file system (PFS) for the initial fetch.


.. _dyad_metadata_key:

Scalable Metadata Key
=====================

.. figure:: _static/figs/DYAD_KEY.png
   :alt: Scalable Metadata Key

DYAD maintains a mapping of ``<filename, owner_rank>`` pairs in the Flux
Key-Value Store (KVS). Naively storing and searching filenames as raw
strings would require byte-by-byte comparison for every lookup, which
becomes expensive as the number of tracked files grows. DYAD instead uses
a hierarchical key structure that enables early termination of costly
string comparisons through a sequence of inexpensive hash comparisons.

Each filename is hashed multiple times using different seed values,
producing a sequence of hash values. These hash values form a multi-level
key hierarchy: at each level, only entries whose hash matches the query
hash at that level are considered for the next level. A mismatch at any
level immediately eliminates the candidate without requiring further
comparison. Only when all hash values match, a full byte-by-byte string
comparison is performed to rule out hash collision. This structure trades
a small amount of storage overhead for a significant reduction in the
number of full string comparisons, making metadata lookups scalable as
the number of tracked files increases. Because each level uses an
independent hash seed, the probability that two distinct filenames produce
matching hashes at every level simultaneously decreases exponentially
with depth, keeping the false positive rate low even for large file sets.

Two environment variables control the shape of the key hierarchy at
run time, without requiring recompilation:

- ``DYAD_KEY_BINS`` — the number of hash bins at each level of the
  hierarchy. A larger value reduces the probability of hash collisions
  at each level, lowering the number of false positives that proceed to
  deeper levels or full string comparison.

- ``DYAD_KEY_DEPTH`` — the number of levels in the hash hierarchy. A
  greater depth means more hash comparisons are performed before a full
  string comparison, providing more opportunities for early termination
  at the cost of a slightly deeper key structure.

The combination of ``DYAD_KEY_BINS`` and ``DYAD_KEY_DEPTH`` allows users
to tune the trade-off between key space size and lookup performance for
their workload. The environment-based defaults are ``DYAD_KEY_DEPTH=3`` and
``DYAD_KEY_BINS=1024``. The C++ ``dyad_params`` defaults are separate
constructor defaults and do not change the environment-based values.

The :ref:`next section <dyad_local-file-access>` details how concurrent accesses to the same file by
multiple workers — as in Case 1 — are coordinated. It also describes
how files are transferred into local storage, which applies to Cases 2,
3, and 4 where the file is not yet present on the requesting worker's
node.
