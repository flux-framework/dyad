#ifndef DYAD_DTL_UCX_H
#define DYAD_DTL_UCX_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <stdlib.h>
#include <ucp/api/ucp.h>

#include <dyad/dtl/dyad_dtl_api.h>
#include <dyad/dtl/ucx_ep_cache.h>

struct dyad_dtl_ucx {
    flux_t *h;                       ///< Non-owning Flux handle, borrowed from @c ctx->h.
    dyad_dtl_comm_mode_t comm_mode;  ///< Communication direction. @see dyad_dtl_comm_mode_t.
    bool debug;                      ///< If @c true, enables verbose UCX debug logging.

    ucp_context_h ucx_ctx;  ///< UCX context. Initialized by @c ucp_init().
    /**
     * UCX worker. Created with @c UCS_THREAD_MODE_SERIALIZED — all UCX
     * calls must be made from a single thread at a time.
     */
    ucp_worker_h ucx_worker;
    ucp_mem_h mem_handle;  ///< UCX memory handle for the pre-allocated RDMA buffer.
    /**
     * Pointer to the UCX-allocated RDMA buffer. Size is
     * @c max_transfer_size + @c sizeof(size_t). The extra @c sizeof(size_t)
     * bytes hold the file size sentinel prepended by the producer so the
     * consumer can detect when the RDMA push has started.
     */
    void *net_buf;
    size_t max_transfer_size;  ///< Maximum transfer size in bytes (@c UCX_MAX_TRANSFER_SIZE).

    /**
     * This worker's UCX address. Sent to the remote peer via the Flux RPC
     * payload (base64-encoded) during @c dyad_dtl_ucx_rpc_pack() so the
     * producer can create an endpoint back to the consumer.
     * Released via @c ucp_worker_release_address() during finalization.
     */
    ucp_address_t *local_address;
    size_t local_addr_len;  ///< Length of @c local_address in bytes.
    /**
     * Decoded UCX address of the remote peer. Producer side only — decoded
     * from base64 in @c dyad_dtl_ucx_rpc_unpack() and allocated via
     * @c malloc(). Released via @c free() during finalization. Not used
     * on the consumer side.
     */
    ucp_address_t *remote_address;
    size_t remote_addr_len;  ///< Length of @c remote_address in bytes.

    /**
     * Active UCX endpoint to the consumer. Non-owning — managed by
     * @c ep_cache. Set to @c NULL between transfers and after finalization.
     */
    ucp_ep_h ep;
    ucp_tag_t comm_tag;  ///< Communication tag: @c tag_prod << 32 | @c tag_cons.
                         ///<   Reset to 0 after each transfer.
    /**
     * Endpoint cache keyed by remote worker address. Avoids recreating
     * @c ucp_ep_h objects for repeated transfers to the same consumer,
     * since UCX endpoint creation involves connection establishment and
     * is expensive relative to the transfer itself.
     * Released via @c dyad_ucx_ep_cache_finalize().
     */
    ucx_ep_cache_h ep_cache;
    ucp_tag_t consumer_conn_key;  ///< Endpoint cache lookup key: @c pid << 32 | @c tag_cons.

    /**
     * Packed remote key buffer for the consumer's RDMA-registered memory.
     * On the producer side, allocated by @c ucp_rkey_pack() in
     * @c ucx_allocate_buffer() — must be released via
     * @c ucp_rkey_buffer_release(). On the consumer side, allocated by
     * @c malloc() in @c dyad_dtl_ucx_rpc_unpack() — must be released
     * via @c free(). The release method therefore depends on
     * @c comm_mode.
     */
    void *rkey_buf;
    size_t rkey_size;  ///< Size of @c rkey_buf in bytes.
    /**
     * Remote destination address of the consumer's RDMA buffer. Used as
     * the target address for @c ucp_put_nbx() in @c ucx_send_no_wait().
     * Extracted from the Flux RPC payload in @c dyad_dtl_ucx_rpc_unpack().
     */
    uint64_t cons_buf_ptr;

    /**
     * Unpacked remote key handle. Unpacked per-transfer in
     * @c ucx_send_no_wait() via @c ucp_ep_rkey_unpack() and destroyed
     * after each send via @c ucp_rkey_destroy(). Must be initialized to
     * @c NULL in @c dyad_dtl_ucx_init() to prevent passing an
     * uninitialized handle to @c ucp_rkey_destroy().
     */
    ucp_rkey_h rkey;
};

typedef struct dyad_dtl_ucx dyad_dtl_ucx_t;

/**
 * @brief Initializes the UCX DTL internal state.
 *
 * @details
 * Allocates and populates the @c dyad_dtl_ucx internal state struct,
 * initializes the UCX context and worker, allocates the RDMA-registered
 * buffer, performs a connection warmup, and wires all function pointers
 * in @c ctx->dtl_handle to their UCX implementations:
 *
 * - @c rpc_pack             → @c dyad_dtl_ucx_rpc_pack
 * - @c rpc_unpack           → @c dyad_dtl_ucx_rpc_unpack
 * - @c rpc_respond          → @c dyad_dtl_ucx_rpc_respond
 * - @c rpc_recv_response    → @c dyad_dtl_ucx_rpc_recv_response
 * - @c get_buffer           → @c dyad_dtl_ucx_get_buffer
 * - @c return_buffer        → @c dyad_dtl_ucx_return_buffer
 * - @c establish_connection → @c dyad_dtl_ucx_establish_connection
 * - @c send                 → @c dyad_dtl_ucx_send
 * - @c recv                 → @c dyad_dtl_ucx_recv
 * - @c close_connection     → @c dyad_dtl_ucx_close_connection
 *
 * Initialization proceeds in the following order:
 *
 *  1. Allocates the @c dyad_dtl_ucx struct and initializes all fields
 *     to safe defaults (@c NULL, 0, or @c UCX_MAX_TRANSFER_SIZE).
 *     The Flux handle is borrowed from @c ctx->h as a non-owning pointer.
 *  2. Reads the UCX configuration via @c ucp_config_read().
 *  3. Initializes the UCX context via @c ucp_init() with the following
 *     features enabled:
 *     - @c UCP_FEATURE_RMA — Remote Memory Access for RDMA push.
 *     - @c UCP_FEATURE_AMO32 — 32-bit atomic memory operations.
 *     - @c UCP_FEATURE_TAG — Tag-matching send/receive.
 *     The request size is set to @c sizeof(dyad_ucx_request_t) and
 *     @c dyad_ucx_request_init is registered as the request initializer.
 *     If @p debug is @c true, the UCX configuration is printed to
 *     @c stderr before the config is released.
 *  4. Creates a UCX worker via @c ucp_worker_create() with
 *     @c UCS_THREAD_MODE_SERIALIZED — all UCX calls must be made from
 *     a single thread at a time.
 *  5. Queries the worker's local address via @c ucp_worker_get_address(),
 *     storing it in @c dtl_handle->local_address. This address is sent
 *     to the remote peer during connection establishment so the peer
 *     can create an endpoint back to this worker.
 *  6. Initializes the endpoint cache via @c dyad_ucx_ep_cache_init().
 *     The cache stores @c ucp_ep_h endpoints keyed by remote worker
 *     address to avoid recreating endpoints for repeated transfers to
 *     the same peer.
 *  7. Allocates and registers an RDMA buffer of
 *     @c UCX_MAX_TRANSFER_SIZE + @c sizeof(size_t) bytes via
 *     @c ucx_allocate_buffer(). The extra @c sizeof(size_t) bytes hold
 *     a prepended file size sentinel used by the consumer to detect
 *     when the producer has started the RDMA push. On the consumer side
 *     (@c DYAD_COMM_RECV), also packs the registered memory region into
 *     @c rkey_buf via @c ucp_rkey_pack() so the packed key can be sent
 *     to the producer to authorize the RDMA push. On the producer side
 *     (@c DYAD_COMM_SEND), @c rkey_buf is left as @c NULL since no peer
 *     performs RDMA into the producer's buffer; it will be populated per
 *     transfer in @c dyad_dtl_ucx_rpc_unpack() with the consumer's
 *     packed key decoded from the RPC payload.
 *  8. Wires all DTL function pointers.
 *  9. Performs a loopback connection warmup via @c ucx_warmup() to
 *     prime the UCX connection machinery before the first real transfer.
 *     After warmup, @c dtl_handle->ep is reset to @c NULL.
 *
 * On any error, @c dyad_dtl_ucx_finalize() is called to clean up
 * partially initialized state before returning.
 *
 * @param[in] ctx       DYAD context. @c ctx->dtl_handle must already be
 *                      allocated by @c dyad_dtl_init().
 * @param[in] mode      DTL mode (must be @c DYAD_DTL_UCX). Redundant
 *                      since @c dyad_dtl_init() already stores the mode
 *                      in @c ctx->dtl_handle->mode before dispatch
 *                      (see TODO).
 * @param[in] comm_mode Communication direction. Controls the memory
 *                      protection flags for the RDMA buffer:
 *                      @c DYAD_COMM_SEND (producer) uses
 *                      @c UCP_MEM_MAP_PROT_LOCAL_READ;
 *                      @c DYAD_COMM_RECV (consumer) uses
 *                      @c UCP_MEM_MAP_PROT_REMOTE_WRITE.
 * @param[in] debug     If @c true, prints the UCX configuration to
 *                      @c stderr and enables verbose debug logging.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK            Initialization succeeded.
 * @retval DYAD_RC_SYSFAIL       Failed to allocate the @c dyad_dtl_ucx
 *                               struct.
 * @retval DYAD_RC_UCXINIT_FAIL  Any other initialization step failed,
 *                               including @c ucp_config_read(),
 *                               @c ucp_init(), @c ucp_worker_create(),
 *                               endpoint cache initialization,
 *                               buffer allocation, or warmup.
 *
 * @todo Remove @p mode parameter — it is already stored in
 *       @c ctx->dtl_handle->mode by @c dyad_dtl_init() before this
 *       function is called.
 */
dyad_rc_t dyad_dtl_ucx_init (const dyad_ctx_t *ctx,
                             dyad_dtl_mode_t mode,
                             dyad_dtl_comm_mode_t comm_mode,
                             bool debug);

/**
 * @brief Packs a file fetch request into a JSON object for a UCX RPC call.
 *
 * @details
 * Creates a Jansson JSON object containing all information the producer
 * needs to locate the file and perform an RDMA push into the consumer's
 * pre-registered buffer. Before packing, resets the first
 * @c sizeof(ssize_t) bytes of @c dtl_handle->net_buf to zero so that
 * @c ucx_recv_no_wait() can use this as a sentinel to detect when the
 * producer has started writing.
 *
 * The packed JSON object contains the following fields:
 *
 * - @c "upath"    — relative path of the file to fetch.
 * - @c "tag_prod" — Flux rank of the producer broker.
 * - @c "cons_buf" — consumer's RDMA buffer address (@c cons_buf_ptr)
 *                   encoded as a decimal string. The producer uses this
 *                   as the remote destination address for @c ucp_put_nbx().
 * - @c "pid_cons" — process ID of the consumer.
 * - @c "addr"     — consumer's UCX worker address, base64-encoded using
 *                   RFC 4648. The producer calls @c ucp_worker_get_address()
 *                   on this to create an endpoint back to the consumer.
 * - @c "rkey"     — consumer's UCX remote key (@c rkey_buf), base64-encoded
 *                   using RFC 4648. The producer calls @c ucp_ep_rkey_unpack()
 *                   on this to obtain the @c ucp_rkey_t needed for
 *                   @c ucp_put_nbx().
 *
 * Both the UCX worker address and the remote key are opaque binary
 * blobs that cannot be embedded directly in JSON. They are base64-encoded
 * (RFC 4648) before packing and decoded by the producer in
 * @c dyad_dtl_ucx_rpc_unpack(). The encoded buffers are freed after
 * @c json_pack() copies them into the JSON object.
 *
 * @note The UCX backend packs significantly more information than the
 *       Flux RPC or Margo backends because RDMA push requires the
 *       producer to know the consumer's exact memory address and
 *       access credentials before initiating the transfer.
 *
 * @param[in]  ctx           DYAD context.
 * @param[in]  upath         Relative path of the file to fetch.
 * @param[in]  producer_rank Flux rank of the producer broker.
 * @param[out] packed_obj    Set to the allocated JSON object on success.
 *                           Undefined on failure. The caller is responsible
 *                           for decrementing the reference count via
 *                           @c json_decref() when no longer needed.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK      The JSON object was created successfully.
 * @retval DYAD_RC_BADPACK @c dtl_handle->local_address is @c NULL,
 *                         base64 encoding of the address or remote key
 *                         failed, or @c json_pack() failed.
 * @retval DYAD_RC_SYSFAIL Failed to allocate the base64 encoding buffer
 *                         for the address or remote key.
 */
dyad_rc_t dyad_dtl_ucx_rpc_pack (const dyad_ctx_t *ctx,
                                 const char *upath,
                                 uint32_t producer_rank,
                                 json_t **packed_obj);

/**
 * @brief Unpacks a file fetch request from an incoming Flux RPC message
 *        and decodes the consumer's UCX worker address and remote key.
 *
 * @details
 * Extracts the following fields from the JSON payload of @p msg using
 * @c flux_request_unpack():
 *
 * - @c "upath"    — relative path of the requested file.
 * - @c "tag_prod" — Flux rank of the producer broker.
 * - @c "cons_buf" — consumer's RDMA buffer address as a decimal string.
 *                   Parsed via @c atoll() and stored in
 *                   @c dtl_handle->cons_buf_ptr as the remote destination
 *                   address for @c ucp_put_nbx().
 * - @c "pid_cons" — process ID of the consumer. Combined with
 *                   @c tag_cons to form @c dtl_handle->consumer_conn_key,
 *                   used as the endpoint cache lookup key.
 * - @c "addr"     — base64-encoded (RFC 4648) UCX worker address of the
 *                   consumer. Decoded into a newly allocated
 *                   @c dtl_handle->remote_address buffer.
 * - @c "rkey"     — base64-encoded (RFC 4648) UCX remote key. Decoded
 *                   into a newly allocated @c dtl_handle->rkey_buf buffer.
 *
 * After unpacking, both the consumer's UCX worker address and remote key
 * are base64-decoded from RFC 4648 encoding. The decoded address is used
 * by @c dyad_dtl_ucx_establish_connection() to create a @c ucp_ep_h
 * endpoint to the consumer (or retrieve a cached one). The decoded remote
 * key is used by @c ucx_send_no_wait() via @c ucp_ep_rkey_unpack() to
 * obtain the @c ucp_rkey_t needed for @c ucp_put_nbx().
 *
 * The communication tag is computed as:
 * @c dtl_handle->comm_tag = tag_prod << 32 | tag_cons
 *
 * The endpoint cache key is computed as:
 * @c dtl_handle->consumer_conn_key = pid << 32 | tag_cons
 *
 * On any decoding failure the corresponding allocated buffer is freed
 * and set to @c NULL before returning.
 *
 * @note Unlike the Margo backend which calls @c margo_addr_lookup() to
 *       resolve the consumer's address, the UCX backend allocates a raw
 *       buffer for the decoded binary address and passes it directly to
 *       @c ucp_worker_get_address() during connection establishment.
 *
 * @note @p upath is owned by the Flux message @p msg and must not be
 *       freed by the caller. It remains valid only for the lifetime
 *       of @p msg.
 *
 * @param[in]  ctx   DYAD context.
 * @param[in]  msg   Incoming Flux RPC message containing the JSON payload
 *                   packed by @c dyad_dtl_ucx_rpc_pack().
 * @param[out] upath Set to the relative path of the requested file.
 *                   Valid for the lifetime of @p msg.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK           Unpacking and decoding succeeded.
 * @retval DYAD_RC_BADUNPACK    @c flux_request_unpack() failed.
 * @retval DYAD_RC_SYSFAIL      Failed to allocate the buffer for the
 *                              decoded address or remote key.
 * @retval DYAD_RC_BAD_B64DECODE Base64 decoding of the address or
 *                              remote key failed.
 */
dyad_rc_t dyad_dtl_ucx_rpc_unpack (const dyad_ctx_t *ctx, const flux_msg_t *msg, char **upath);

/**
 * @brief Sends the initial RPC acknowledgement from the service to the consumer.
 *
 * @details
 * No-op for the UCX DTL. As with the Margo backend, no explicit
 * acknowledgement over the Flux RPC channel is needed before data
 * transfer begins — the producer connects directly to the consumer's
 * UCX worker address and pushes data via @c ucp_put_nbx().
 *
 * @param[in] ctx      DYAD context.
 * @param[in] orig_msg Unused by this backend.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_ucx_rpc_respond (const dyad_ctx_t *ctx, const flux_msg_t *orig_msg);

/**
 * @brief Receives the initial RPC response from the service.
 *
 * @details
 * No-op for the UCX DTL. The consumer does not process a Flux RPC
 * response before data transfer begins — it waits directly on the
 * sentinel value in @c dtl_handle->net_buf via @c ucx_recv_no_wait(),
 * which polls until the producer's RDMA push has started writing data.
 *
 * @param[in] ctx Unused by this backend.
 * @param[in] f   Unused by this backend.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_ucx_rpc_recv_response (const dyad_ctx_t *ctx, flux_future_t *f);

/**
 * @brief Returns a pointer to the pre-allocated UCX RDMA buffer.
 *
 * @details
 * Unlike the Flux RPC and Margo backends which allocate a new buffer
 * on each call, the UCX backend uses a single pre-allocated RDMA-
 * registered buffer (@c dtl_handle->net_buf) allocated during
 * @c dyad_dtl_ucx_init(). This function simply validates @p data_size
 * and sets @p *data_buf to point to that pre-allocated buffer.
 *
 * Reusing a pre-allocated buffer avoids the overhead of repeated
 * @c ucp_mem_map() registrations, which are expensive because they
 * pin memory and register it with the network hardware.
 *
 * @note The buffer validation check (@p data_buf == @c NULL or
 *       @p *data_buf != @c NULL) is currently disabled due to a known
 *       issue where the @p *data_buf != @c NULL check incorrectly
 *       evaluates to @c true even when @p data_buf points to a @c NULL
 *       pointer (see TODO in source).
 *
 * @note The caller must not free @p *data_buf — it points into the
 *       UCX-registered memory region managed by @c dtl_handle and must
 *       be released via @c dyad_dtl_ucx_return_buffer() which is itself
 *       a no-op for UCX (the buffer is only freed during finalization).
 *
 * @todo Investigate and fix the @p *data_buf != @c NULL validation
 *       check that incorrectly fires for valid @c NULL-initialized
 *       pointers.
 *
 * @param[in]  ctx       DYAD context.
 * @param[in]  data_size Number of bytes needed. Must not exceed
 *                       @c dtl_handle->max_transfer_size
 *                       (@c UCX_MAX_TRANSFER_SIZE).
 * @param[out] data_buf  Set to @c dtl_handle->net_buf on success.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK     @p *data_buf set to the pre-allocated buffer.
 * @retval DYAD_RC_BADBUF @p data_size exceeds @c max_transfer_size.
 */
dyad_rc_t dyad_dtl_ucx_get_buffer (const dyad_ctx_t *ctx, size_t data_size, void **data_buf);

/**
 * @brief Releases a reference to the UCX pre-allocated RDMA buffer.
 *
 * @details
 * Unlike the Flux RPC and Margo backends which @c free() the buffer,
 * the UCX backend only sets @p *data_buf to @c NULL since the buffer
 * is pre-allocated and RDMA-registered during @c dyad_dtl_ucx_init()
 * and must persist for the lifetime of the DTL handle. The actual
 * memory is released by @c ucx_free_buffer() during finalization.
 *
 * Validates @p data_buf before clearing:
 * - If @p data_buf is @c NULL or @p *data_buf is @c NULL, returns
 *   @c DYAD_RC_BADBUF.
 *
 * @param[in,out] ctx      DYAD context.
 * @param[in,out] data_buf Pointer to the buffer pointer to clear.
 *                         @p *data_buf is set to @c NULL on success.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK     @p *data_buf cleared successfully.
 * @retval DYAD_RC_BADBUF @p data_buf or @p *data_buf is @c NULL.
 */
dyad_rc_t dyad_dtl_ucx_return_buffer (const dyad_ctx_t *ctx, void **data_buf);

/**
 * @brief Establishes a UCX endpoint connection for data transfer.
 *
 * @details
 * Behavior differs by communication direction:
 *
 * - @c DYAD_COMM_SEND (producer): Looks up the consumer's endpoint
 *   in the endpoint cache via @c dyad_ucx_ep_cache_find() using
 *   @c dtl_handle->remote_address as the key. If no cached endpoint
 *   exists, creates a new one via @c dyad_ucx_ep_cache_insert(), which
 *   calls @c ucp_ep_create() and stores the result in the cache.
 *   The endpoint is stored in @c dtl_handle->ep for use by
 *   @c ucx_send_no_wait(). If @p debug is @c true, prints endpoint
 *   information to @c stderr via @c ucp_ep_print_info().
 *
 * - @c DYAD_COMM_RECV (consumer): No-op. The consumer does not need
 *   to create an endpoint — it passively waits for the producer to
 *   push data into the pre-registered RDMA buffer via @c ucp_put_nbx().
 *
 * The endpoint cache avoids recreating @c ucp_ep_h objects for repeated
 * transfers to the same consumer, which is significant because UCX
 * endpoint creation involves connection establishment with the remote
 * worker and is expensive relative to the data transfer itself.
 *
 * @param[in] ctx DYAD context. The remote address, endpoint cache,
 *                UCX worker, and endpoint pointer are read from and
 *                written to the UCX DTL internal state.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK          Connection established or not needed
 *                             (consumer side).
 * @retval DYAD_RC_BAD_COMM_MODE @c dtl_handle->comm_mode is invalid.
 * @retval other               Any error code returned by
 *                             @c dyad_ucx_ep_cache_insert() if
 *                             endpoint creation failed.
 */
dyad_rc_t dyad_dtl_ucx_establish_connection (const dyad_ctx_t *ctx);

/**
 * @brief Sends file data to the consumer via UCX RDMA push.
 *
 * @details
 * Initiates a non-blocking RDMA put via @c ucx_send_no_wait() with
 * @c is_warmup=false, then blocks until the operation completes by
 * calling @c dyad_ucx_request_wait(). The data is pushed directly into
 * the consumer's pre-registered RDMA buffer at
 * @c dtl_handle->cons_buf_ptr using the endpoint and remote key
 * established during @c dyad_dtl_ucx_establish_connection() and
 * @c dyad_dtl_ucx_rpc_unpack().
 *
 * @note The producer prepends the file size to @p buf before calling
 *       this function (in @c dyad_fetch_request_cb()), so the consumer
 *       can use the first @c sizeof(size_t) bytes as a sentinel in
 *       @c ucx_recv_no_wait() to detect when the push has started.
 *
 * @param[in] ctx    DYAD context.
 * @param[in] buf    Buffer containing the file data to send. Must point
 *                   to the pre-allocated UCX-registered buffer
 *                   (@c dtl_handle->net_buf) since @c ucp_put_nbx()
 *                   requires the source buffer to be registered with UCX.
 * @param[in] buflen Number of bytes to send.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK            Data sent successfully.
 * @retval DYAD_RC_UCXCOMM_FAIL  @c ucp_put_nbx() or
 *                               @c dyad_ucx_request_wait() failed.
 */
dyad_rc_t dyad_dtl_ucx_send (const dyad_ctx_t *ctx, void *buf, size_t buflen);

/**
 * @brief Writes the signed @c -1 failure prefix into the consumer's UCX buffer.
 *
 * @param[in] ctx Producer-side UCX DTL context for the accepted request.
 * @return @c DYAD_RC_OK when the marker was delivered; a UCX error otherwise.
 */

/**
 * @brief Receives file data from the producer via UCX RDMA push.
 *
 * @details
 * Calls @c ucx_recv_no_wait() with @c is_warmup=false to busy-poll
 * the first @c sizeof(ssize_t) bytes of @c dtl_handle->net_buf until
 * they become non-zero, indicating that the producer has started the
 * RDMA push. After @c ucx_recv_no_wait() returns, the received data
 * is available directly in @c dtl_handle->net_buf — @p buf and
 * @p buflen are not populated by this function and must be read by
 * the caller from the pre-allocated buffer via
 * @c dyad_dtl_ucx_get_buffer().
 *
 *
 * @note Unlike the Flux RPC backend where @p buf and @p buflen are
 *       populated with a newly allocated copy of the received data,
 *       the UCX backend leaves data in the pre-allocated
 *       @c dtl_handle->net_buf. The caller reads it directly from
 *       there without an additional copy.
 *
 * @param[in]  ctx    DYAD context.
 * @param[out] buf    Not populated by this function. The received data
 *                    is in @c dtl_handle->net_buf.
 * @param[out] buflen Not populated by this function.
 *
 * @return Always returns @c DYAD_RC_OK after the prefix arrives.
 *
 * @todo Add a timeout for the case where the prefix never arrives.
 */
dyad_rc_t dyad_dtl_ucx_recv (const dyad_ctx_t *ctx, void **buf, size_t *buflen);

/**
 * @brief Closes the UCX DTL data channel after a transfer completes.
 *
 * @details
 * Behavior differs by communication direction:
 *
 * - @c DYAD_COMM_SEND (producer): Destroys the unpacked remote key
 *   via @c ucp_rkey_destroy() — the remote key is unpacked per-transfer
 *   in @c ucx_send_no_wait() and must be destroyed after each send.
 *   Clears @c dtl_handle->ep, @c dtl_handle->remote_address,
 *   @c dtl_handle->remote_addr_len, and @c dtl_handle->comm_tag.
 *
 *   The endpoint itself is @b not disconnected — it is retained in the
 *   endpoint cache for reuse in future transfers to the same consumer,
 *   avoiding the cost of reconnection. LRU eviction of stale endpoints
 *   from the cache is not yet implemented (see TODO).
 *
 *   @c dtl_handle->remote_address is set to @c NULL but not freed —
 *   the pointer is still referenced by the endpoint cache entry and
 *   must remain valid until the cache entry is evicted (see TODO).
 *
 * - @c DYAD_COMM_RECV (consumer): No-op beyond resetting
 *   @c dtl_handle->comm_tag to 0. The consumer has no endpoint to
 *   close since it passively receives data via the pre-registered
 *   RDMA buffer without creating a @c ucp_ep_h.
 *
 * @param[in] ctx DYAD context.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK          Connection closed successfully.
 * @retval DYAD_RC_BAD_COMM_MODE @c dtl_handle->comm_mode is invalid.
 *
 * @todo Implement LRU eviction for the endpoint cache to reclaim
 *       resources for endpoints that have not been used recently.
 *       The commented-out @c ucx_disconnect() call shows the intended
 *       disconnect path that would be used on eviction.
 *
 * @todo Free @c dtl_handle->remote_address here once the endpoint
 *       cache no longer references it, either by copying the address
 *       into the cache entry or by reference-counting it.
 */
dyad_rc_t dyad_dtl_ucx_close_connection (const dyad_ctx_t *ctx);

/**
 * @brief Finalizes and frees the UCX DTL internal state.
 *
 * @details
 * Releases all UCX resources in the correct teardown order, guarding
 * each step with a @c NULL check to allow safe partial finalization
 * when called after a failed @c dyad_dtl_ucx_init():
 *
 *  1. If @c dtl_handle->ep is non-@c NULL, closes the active connection
 *     via @c dyad_dtl_ucx_close_connection() to destroy the unpacked
 *     remote key and clear per-RPC connection state, then sets
 *     @c ep to @c NULL.
 *  2. If @c dtl_handle->ep_cache is non-@c NULL, finalizes the endpoint
 *     cache via @c dyad_ucx_ep_cache_finalize(), which disconnects and
 *     destroys all cached @c ucp_ep_h endpoints, then sets
 *     @c ep_cache to @c NULL. This is the actual connection teardown —
 *     endpoints are kept alive across RPCs in the cache and are only
 *     destroyed here.
 *  3. If @c dtl_handle->local_address is non-@c NULL, releases the
 *     local worker address via @c ucp_worker_release_address() and
 *     sets it to @c NULL. @c ucp_worker_release_address() must be used
 *     instead of @c free() since the address is UCX-allocated by
 *     @c ucp_worker_get_address().
 *  4. If @c dtl_handle->remote_address is non-@c NULL, frees it with
 *     @c free() and sets it to @c NULL. This handles the case where
 *     @c dyad_dtl_ucx_close_connection() was skipped on an error path
 *     and @c remote_address was not freed there.
 *  5. If @c dtl_handle->rkey is non-@c NULL, destroys the unpacked
 *     remote key handle via @c ucp_rkey_destroy() and sets it to
 *     @c NULL. This is normally @c NULL at finalization since
 *     @c dyad_dtl_ucx_close_connection() destroys it after each
 *     transfer. A non-@c NULL value indicates that
 *     @c close_connection() was skipped on an error path.
 *  6. If @c dtl_handle->rkey_buf is non-@c NULL, releases it and sets
 *     it to @c NULL. The release function differs by @c comm_mode:
 *     @c DYAD_COMM_SEND uses @c free() since @c rkey_buf is a plain
 *     @c malloc() allocation from @c dyad_dtl_ucx_rpc_unpack();
 *     @c DYAD_COMM_RECV uses @c ucp_rkey_buffer_release() since
 *     @c rkey_buf is UCX-allocated by @c ucp_rkey_pack() in
 *     @c ucx_allocate_buffer().
 *  7. If @c dtl_handle->mem_handle is non-@c NULL, unmaps and frees the
 *     pre-allocated RDMA buffer via @c ucx_free_buffer(), which calls
 *     @c ucp_mem_unmap() and sets @c dtl_handle->net_buf to @c NULL.
 *     Then sets @c mem_handle to @c NULL.
 *  8. If @c dtl_handle->ucx_worker is non-@c NULL, destroys the UCX
 *     worker via @c ucp_worker_destroy() and sets it to @c NULL.
 *  9. If @c dtl_handle->ucx_ctx is non-@c NULL, releases the UCX
 *     context via @c ucp_cleanup() and sets it to @c NULL.
 * 10. Sets @c dtl_handle->h to @c NULL. The Flux handle is non-owning
 *     and must not be closed here — it is managed by the DYAD context.
 * 11. Frees the @c dyad_dtl_ucx struct and sets the handle pointer
 *     to @c NULL.
 *
 * If @c ctx->dtl_handle is @c NULL or
 * @c ctx->dtl_handle->private_dtl.ucx_dtl_handle is @c NULL, the
 * function is a no-op and returns @c DYAD_RC_OK.
 *
 * @note The endpoint cache must be finalized before the UCX worker is
 *       destroyed, since endpoint disconnection requires the worker to
 *       be active to flush pending operations.
 *
 * @note @c rkey and @c rkey_buf must be released before the UCX worker
 *       and context are destroyed since they are UCX-managed resources
 *       bound to the UCX context.
 *
 * @note The RDMA buffer must be unmapped before the UCX context is
 *       cleaned up, since @c ucp_mem_unmap() requires a valid UCX
 *       context.
 *
 * @note This function only frees the @c dyad_dtl_ucx struct. The
 *       outer @c dyad_dtl handle is freed by @c dyad_dtl_finalize(),
 *       which calls this function as part of the full teardown sequence.
 *
 * @param[in] ctx DYAD context. On return,
 *                @c ctx->dtl_handle->private_dtl.ucx_dtl_handle
 *                is @c NULL.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_ucx_finalize (const dyad_ctx_t *ctx);

#endif /* DYAD_DTL_UCX_H */
