.. _DYAD_runtime_config:

******************************
Runtime Configuration
******************************

At launch time, DYAD allows users to customize its behavior and make better use
of the environment through various environment variables. The list of these 
variables is shown below.

+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| Name                               | Type            | Required?    | Default  | Description                                                     |
+====================================+=================+==============+==========+=================================================================+
| :code:`DYAD_KVS_NAMESPACE`         | String          | Yes          | N/A      | The Flux KVS namespace that DYAD will use to record or look     |
|                                    |                 |              |          |                                                                 |
|                                    |                 |              |          | for file information                                            |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_PATH_PRODUCER`         | Directory Path  | Yes [#two]_  | N/A      | The producer-managed path of the application                    |
+------------------------------------+                 +              +          +-----------------------------------------------------------------+
| :code:`DYAD_PATH_CONSUMER`         |                 |              |          | The consumer-managed path of the application                    |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_DTL_MODE`              | String          | No           | FLUX_RPC | Choose data transfer method among MARGO, UCX, FLUX_RPC          |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_MARGO_PROTO` [#thr]_   | String          | No           | ofi\+tcp | Specify network protocol when :code:`DYAD_DTL_MODE=MARGO`       |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_PATH_RELATIVE`         | Set or unset    | No           | Unset    | When set (including ``=0``), relative paths are interpreted     |
|                                    |                 |              |          | relative to the managed directory.                              |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_SHARED_STORAGE`        | Set or unset    | No           | Unset    | When set (including ``=0``), DYAD synchronizes access on shared |
|                                    |                 |              |          | storage but does not transfer file data.                        |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_ASYNC_PUBLISH`         | Set or unset    | No           | Unset    | When set (including ``=0``), publish metadata asynchronously.   |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_SERVICE_MUX`           | integer >= 1    | No           | 1        | Number of Flux brokers sharing node-local storage.              |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_KEY_DEPTH` [#for]_     | Integer         | No           | 3        | The number of levels in Flux's hierarchical KVS to use          |
|                                    |                 |              |          |                                                                 |
|                                    |                 |              |          | within DYAD's namespace                                         |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_KEY_BINS` [#for]_      | Integer         | No           | 1024     | The maximum number of unique hash values per level in Flux's    |
|                                    |                 |              |          |                                                                 |
|                                    |                 |              |          | hierarchical KVS within DYAD's namespace.                       |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_CACHE_CAPACITY`        | Integer (bytes) | No           | 0        | Maximum bytes DYAD may use in a managed directory before        |
|                                    |                 |              |          | evicting files. 0 (default) disables eviction entirely.         |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_CACHE_POLICY`          | String          | No           | LRU      | Eviction policy: :code:`LRU`, :code:`FIFO`, or :code:`NONE`.    |
|                                    |                 |              |          | Ignored if :code:`DYAD_CACHE_CAPACITY` is 0.                    |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_CACHE_LOW_WATERMARK`   | Float (0..1)    | No           | 0.8      | Fraction of :code:`DYAD_CACHE_CAPACITY` to evict down to once   |
|                                    |                 |              |          | eviction triggers.                                              |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_CACHE_GRACE_PERIOD`    | Integer (sec)   | No           | 5        | Skip eviction candidates accessed more recently than this many  |
|                                    |                 |              |          | seconds.                                                        |
+------------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+

Additional controls
-------------------

- ``DYAD_FSYNC_WRITE``: presence-based switch that makes producers call
  ``fsync()`` before metadata publication.
- ``DYAD_SYNC_DEBUG`` and ``DYAD_SYNC_CHECK``: presence-based switches for
  synchronization debug logging and health checks.
- ``DYAD_REINIT``: presence-based switch that forces context
  reinitialization when an initialization entry point is called again.
- ``DYAD_GOTCHA_PRIORITY``: integer priority for the preload wrapper; higher
  priorities run before lower priorities.
- ``DYAD_PATH_ORIGIN``: optional fallback source path used only by lazy
  range-cache fills. Whole-file consumption does not fetch from this path.
- ``DYAD_FETCH_WORKER_THREADS``: positive integer controlling the Flux
  module's range-fetch worker pool; defaults to 8 and is not a client-context
  setting.

.. [#two] For DYAD to do anything, at least one of :code:`DYAD_PATH_PRODUCER` or :code:`DYAD_PATH_CONSUMER` must be provided.
   Applications will still work if neither are provided, but DYAD will not do anything.

.. [#thr] When built with the cmake option :code:`DYAD_ENABLE_MARGO_DATA=ON` and run
   with the env variable :code:`DYAD_DTL_MODE=MARGO`, :code:`DYAD_MARGO_PROTO`
   specifies the network protocol. See the table below for example values.

.. note::

   ``DYAD_PATH_RELATIVE``, ``DYAD_SHARED_STORAGE``, and
   ``DYAD_ASYNC_PUBLISH`` are presence-based switches. Setting one to ``0``
   still enables it; leave it unset to disable it.

.. [#for] The Flux KVS supports organized categorization of data through hierarchical `key structuring <https://flux-framework.readthedocs.io/projects/flux-core/en/latest/man1/flux-kvs.html>`_. DYAD leverages this capability to optimize hash lookup performance by increasing the likelihood of early search termination when no matching entry exists in the store. The search key is hashed across multiple levels using different seeds. A match is confirmed only if corresponding entries are found at all levels, with an exact key-string match at the final level.

**Examples of valid values for DYAD_MARGO_PROTO:**

+---------------------+--------------------------------------------------------+
| Value               | Description                                            |
+=====================+========================================================+
| ``ofi+tcp``         | Portable TCP/IP via libfabric (default)                |
+---------------------+--------------------------------------------------------+
| ``ofi+verbs``       | InfiniBand via libfabric                               |
+---------------------+--------------------------------------------------------+
| ``ofi+cxi``         | HPE Slingshot (Cray EX) via libfabric                  |
+---------------------+--------------------------------------------------------+
| ``na+sm``           | Shared memory (current Margo; use ``sm`` for older)    |
+---------------------+--------------------------------------------------------+
| ``ucx+all://``      | Auto; let UCX pick, safe default, often doesn't work   |
+---------------------+--------------------------------------------------------+
| ``ucx+tcp://``      | TCP/IP, portable                                       |
+---------------------+--------------------------------------------------------+
| ``ucx+rc_v://``     | InfiniBand RC (verbs), low-latency IB                  |
+---------------------+--------------------------------------------------------+
| ``ucx+rc_mlx5://``  | InfiniBand RC (mlx5), Mellanox HCAs                    |
+---------------------+--------------------------------------------------------+
| ``ucx+ud_v://``     | InfiniBand UD (verbs), scalable IB                     |
+---------------------+--------------------------------------------------------+
| ``ucx+dc_mlx5://``  | InfiniBand DC (mlx5), large-scale IB (Frontier, Sierra)|
+---------------------+--------------------------------------------------------+
| ``ucx+cma://``      | Cross-Memory Attach, intra-node shared memory (Linux)  |
+---------------------+--------------------------------------------------------+
| ``ucx+sysv://``     | SysV shared memory, intra-node only                    |
+---------------------+--------------------------------------------------------+
