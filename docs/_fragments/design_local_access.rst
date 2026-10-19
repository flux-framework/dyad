.. _dyad_local-file-access:

Local file access coordination via locking
##########################################

To allow seamless coordination between producers and consumers of files,
DYAD supports both intercepting POSIX I/O using GOTCHA and wrapper classes
for C++ streams. The two methods currently operate in a mutually exclusive
fashion. To assure the soundness of access on files that are visible to
multiple workers including producers and consumers, we rely on file locking
mechanisms.

.. figure:: _static/figs/dyad_locking_summary.svg
   :alt: DYAD locking protocol summary

   This diagram shows the flow of each of the two independent methods.
   The C GOTCHA wrappers intercept POSIX-level calls while the C++ stream
   wrappers operate independently. In both paths the producer holds an
   exclusive lock for the duration of the write to protect consumers with
   direct file visibility from reading premature files.
   The consumer acquires an exclusive OFD lock inside ``dyad_consume()`` to
   coordinate with local producers and serialize concurrent fetches. On a
   successful read path, it is downgraded to a shared guard. The guard remains
   held through the real application open until a shared OFD lock is installed
   on the application descriptor; the guard is then closed.

.. figure:: _static/figs/dyad_consumer_race_fetch.svg
   :alt: Consumer race management — file fetched from remote producer

   The exclusive lock serializes consumers so that only one performs the
   expensive KVS wait and data fetch while avoiding to overwrite the file.
   When the second consumer eventually acquires the lock, it finds the file
   already present (size > 0) and skips the fetch entirely.
   The full sequence for consumer A is: fetch metadata ->
   data transfer (``dyad_get_data()``) -> write to disk (``dyad_cons_store()``)
   -> cache eviction -> downgrade to a shared guard -> real reader open. The
   guard remains held until the real reader descriptor has its shared lock,
   with consumer B blocked throughout the fetch and handoff.

.. figure:: _static/figs/dyad_consumer_local_file.svg
   :alt: Consumer race management — file already local

   If the file is already present locally, there is no need for fetching
   metadata or the file itself. The consumer still downgrades its lock to the
   shared guard before the real read-only open, preserving the same lock
   handoff as a fetched file.


.. _dyad_local-file-access_fallback:

Fallback behavior when filesystem locking is unavailable
========================================================

Filesystem locking in the C++ stream wrapper path relies on the ability to
extract the underlying file descriptor from a ``std::basic_filebuf`` object
via the GCC/libstdc++ internal ``_M_file.fd()`` member. This capability is
detected automatically at configure time by CMake and exposed as the
``DYAD_HAS_STD_FSTREAM_FD`` compile-time flag.

When ``DYAD_HAS_STD_FSTREAM_FD`` is not defined, filesystem locking is
unavailable in the C++ stream path and ``ctx->use_fs_locks`` is set to
``false``. In this case, ``dyad_consume()`` cannot rely on file size alone
to determine whether a co-located producer has finished writing — since the
producer cannot lock the file it is still writing, a non-zero file size does
not guarantee the file is complete. DYAD therefore falls back to always
consulting the Flux KVS for synchronization regardless of file size, at the
cost of additional network overhead compared to the local locking path.

The C GOTCHA wrapper path is unaffected by this fallback. Since it always
has direct access to file descriptors, ``use_fs_locks`` is explicitly set to
``true`` in ``dyad_wrapper_init()`` during library initialization.

``dyad_consume()`` is invoked on the consumer side in both interception
paths: in the C GOTCHA wrapper it is called inside the ``open()`` wrapper,
and in the C++ stream path it is called via ``open_sync()`` from
``basic_ifstream_dyad`` and ``basic_fstream_dyad`` constructors and their
``open()`` methods.

Similarly, ``dyad_produce()`` is invoked on the producer side in both paths:
in the C GOTCHA wrapper it is called inside the ``close()`` wrapper, and in
the C++ stream path it is called via ``close_sync()`` from
``basic_ofstream_dyad`` and ``basic_fstream_dyad`` destructors and their
``close()`` methods.


.. _dyad_local-file-access_stream_sync:

Data durability in the C++ stream path
======================================

Unlike the C GOTCHA wrapper, the C++ stream destructor and ``close()``
perform an additional step of flushing kernel-buffered stream data to stable
storage by calling ``fsync()`` on the file descriptor extracted from the
stream. This ensures all written data is durable before ``dyad_produce()``
notifies consumers, providing a stronger durability guarantee than the C
GOTCHA wrapper path which does not perform an explicit ``fsync()``.

