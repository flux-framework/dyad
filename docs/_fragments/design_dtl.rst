.. _dyad_dtl:

Data Transport Layer (DTL)
##########################

The following diagrams show the sequence of DTL function calls for each
backend, starting from ``dyad_get_data()`` on the consumer side through
to the producer service.

General DTL sequence
====================

.. figure:: _static/figs/dyad_dtl_general_sequence.svg
   :alt: General DYAD DTL data transfer sequence

   The abstract DTL interface sequence starting from ``dyad_get_data()``.
   All backends follow this same call order — the implementation of each
   step differs per backend.
   The buffer allocated by ``get_buffer()`` serves different purposes on
   each side of the transfer. On the producer side, the file is read from
   local storage into the buffer, which is then passed to ``send()`` to
   transfer the data to the consumer. On the consumer side, the buffer
   receives the incoming data via ``recv()``, after which
   ``dyad_cons_store()`` writes the buffer contents to the consumer's local
   managed directory. From that point on, the application can read the file
   through its normal I/O path — whether via the GOTCHA wrapper or the C++
   stream wrapper — without any knowledge of the transfer or synchronization
   that took place. To the application, the file simply appears in local
   storage as if it had always been there.

Flux RPC backend
================

.. figure:: _static/figs/dyad_dtl_flux_sequence.svg
   :alt: DYAD Flux RPC DTL data transfer sequence

   The Flux RPC backend routes all data through the Flux broker message
   bus. Several steps are no-ops since the streaming RPC connection is
   implicit. The stored ``flux_msg_t*`` pointer routes ``flux_respond_raw()``
   responses back to the correct consumer.

Margo backend
=============

.. figure:: _static/figs/dyad_dtl_margo_sequence.svg
   :alt: DYAD Margo DTL data transfer sequence

   The Margo backend uses an inverted model — the consumer runs a Margo
   server and the producer connects back to it. Data is transferred via
   ``HG_BULK_PULL``: the producer registers its buffer and triggers the
   consumer's ``data_ready_rpc()`` handler to pull the data via RDMA.

UCX backend
===========

.. figure:: _static/figs/dyad_dtl_ucx_sequence.svg
   :alt: DYAD UCX DTL data transfer sequence

   The UCX backend uses a push model with pre-registered RDMA memory.
   The producer performs a one-sided ``ucp_put_nbx()`` directly into the
   consumer's pre-registered buffer. The consumer polls a size sentinel
   to detect arrival. Endpoints are cached across transfers.

Failure signaling
=================

The producer must notify the consumer when it cannot serve a request; a
consumer must not wait indefinitely for data that will never arrive. Flux RPC
reports the failure on the streaming RPC. UCX writes an ``ssize_t`` prefix of
``-1`` instead of a positive transfer length. Margo sends its
``data_ready_rpc()`` notification with ``n = -1``. Each consumer backend
converts its transport-specific marker into a DYAD error.

DTL backend comparison
======================

All three DTL backends share the same abstract interface — the same
sequence of ``rpc_pack()``, ``rpc_unpack()``, ``rpc_respond()``,
``rpc_recv_response()``, ``establish_connection()``, ``get_buffer()``,
``send()``, ``recv()``, ``return_buffer()``, and ``close_connection()``
calls — but implement them very differently depending on the underlying
transport.

The **Flux RPC backend** is the simplest. Data travels entirely through
the Flux broker message bus: the producer calls ``flux_respond_raw()``
with the file contents and the consumer reads them via
``flux_rpc_get_raw()``. No RDMA, no external library, no connection
setup — the Flux streaming RPC handles everything. The original request
message (``flux_msg_t*``) is stored in the DTL handle after
``rpc_unpack()`` so that ``send()`` can route the response back to the
correct consumer. Several steps are no-ops: ``rpc_respond()``,
``rpc_recv_response()``, and both ``establish_connection()`` calls do
nothing because the streaming RPC connection is implicit. This backend
requires only Flux and is the most portable, but it routes all data
through the broker and cannot exploit RDMA hardware.

The **Margo backend** uses an inverted client-server model. The consumer
initializes Margo in ``MARGO_SERVER_MODE`` during ``dyad_dtl_margo_init()``
so that the producer can connect back to it. The consumer embeds its own
Margo server address (``margo_addr_to_string()``) in the Flux RPC request
payload, which the producer extracts and resolves via ``margo_addr_lookup()``
during ``rpc_unpack()``. The actual data transfer uses ``HG_BULK_PULL`` —
the producer registers its file buffer as read-only, sends a Margo RPC
(``data_ready_rpc``) to the consumer's Margo server, and the consumer's
handler pulls the data directly from the producer's buffer via RDMA. The
consumer's ``recv()`` busy-waits on ``margo_handle->recv_ready`` until
``data_ready_rpc()`` publishes completion. The producer caches resolved consumer
addresses by their Margo address string, while each transfer still creates and
destroys its own Margo RPC handle within ``send()``.

The **UCX backend** uses a push model with pre-registered RDMA memory.
During ``dyad_dtl_ucx_init()``, both producer and consumer allocate a
large buffer (``UCX_MAX_TRANSFER_SIZE + sizeof(size_t)`` bytes) and register
it with UCX via ``ucp_mem_map()``. The consumer packs its buffer address
(``cons_buf_ptr``) and the packed remote key (``rkey_buf``, base64-encoded)
into the Flux RPC request. The producer decodes these during ``rpc_unpack()``,
unpacks the remote key via ``ucp_ep_rkey_unpack()``, and performs a
one-sided RDMA push directly into the consumer's pre-registered memory
via ``ucp_put_nbx()``. The consumer's ``recv()`` polls the first
``sizeof(ssize_t)`` bytes of its buffer — the producer prepends the transfer
length as a sentinel. A positive value announces data, while ``-1`` announces
a producer-side failure.
To avoid repeated endpoint creation, the UCX backend maintains an endpoint
cache (``ucx_ep_cache_h``) keyed by consumer connection key. The
``return_buffer()`` call is a no-op for UCX — the pre-registered buffer
persists for the lifetime of the DTL handle and is only freed during
finalization.
