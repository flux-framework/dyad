#ifndef DYAD_DTL_margo_H
#define DYAD_DTL_margo_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/dtl/dyad_dtl_api.h>
#include <margo.h>
#include <stdatomic.h>
#include <stdlib.h>

struct dyad_dtl_margo {
    flux_t *h;
    bool debug;
    margo_instance_id mid;    // margo id
    hg_addr_t local_addr;     // margo local server address
    hg_addr_t remote_addr;    // margo remote server address
    hg_id_t sendrecv_rpc_id;  // margo rpc id for send/recv
    atomic_bool recv_ready;
    size_t recv_len;
    void *recv_buffer;
    void *bulk_buffer;
    size_t bulk_buffer_size;
    hg_bulk_t bulk_handle;
};

typedef struct dyad_dtl_margo dyad_dtl_margo_t;

/**
 * @brief Initializes the Margo DTL internal state.
 *
 * @details
 * Allocates and populates the @c dyad_dtl_margo internal state struct,
 * initializes the Margo instance, registers the @c data_ready_rpc RPC,
 * and wires all function pointers in @c ctx->dtl_handle to their Margo
 * implementations:
 *
 * - @c rpc_pack             → @c dyad_dtl_margo_rpc_pack
 * - @c rpc_unpack           → @c dyad_dtl_margo_rpc_unpack
 * - @c rpc_respond          → @c dyad_dtl_margo_rpc_respond
 * - @c rpc_recv_response    → @c dyad_dtl_margo_rpc_recv_response
 * - @c get_buffer           → @c dyad_dtl_margo_get_buffer
 * - @c return_buffer        → @c dyad_dtl_margo_return_buffer
 * - @c establish_connection → @c dyad_dtl_margo_establish_connection
 * - @c send                 → @c dyad_dtl_margo_send
 * - @c recv                 → @c dyad_dtl_margo_recv
 * - @c close_connection     → @c dyad_dtl_margo_close_connection
 *
 * The Mercury/NA protocol is read from the @c DYAD_MARGO_PROTO
 * environment variable (@c DYAD_MARGO_PROTO_ENV). If not set, it
 * defaults to @c "ofi+tcp". The protocol is validated via
 * @c validate_margo_protocol() before @c margo_init() is called.
 *
 * Margo is initialized differently depending on @p comm_mode:
 *
 * - @c DYAD_COMM_SEND (producer / Flux broker side): Margo is
 *   initialized in @c MARGO_CLIENT_MODE with no dedicated progress ES
 *   and no RPC handler ES. The @c data_ready_rpc RPC is registered
 *   without a handler (@c NULL), since the producer only sends RPCs.
 *
 * - @c DYAD_COMM_RECV (consumer / client wrapper side): Margo is
 *   initialized in @c MARGO_SERVER_MODE with a dedicated Execution
 *   Stream (ES) for the Mercury progress loop (@c use_progress_thread=1)
 *   and RPC handlers running in that same ES (@c rpc_thread_count=-1).
 *   The RPC named @c "data_ready_rpc" is registered with @c data_ready_rpc()
 *   as its handler function, and the @c margo_handle is registered a
 *   auxiliary data accessible to the handler via @c margo_registered_data().
 * @note  RPC handlers run in the same Argobots Execution Stream (ES) as the
 *        Mercury progress loop (@c rpc_thread_count=-1), meaning no additional
 *        ES is created for handler execution. This is safe because the consumer
 *        blocks in a busy-wait loop on @c margo_handle->recv_ready until
 *        @c data_ready_rpc() signals completion — there is no concurrent work
 *        that could be starved by sharing the progress loop ES with the handler.
 *
 * Both modes retrieve their own local Margo address via
 * @c margo_addr_self() and initialize @c remote_addr to @c NULL
 * (set during connection establishment).
 *
 * On any error, @c dyad_dtl_margo_finalize() is called to clean up
 * partially initialized state before returning.
 *
 * @param[in] ctx       DYAD context. @c ctx->dtl_handle must already be
 *                      allocated by @c dyad_dtl_init().
 * @param[in] mode      DTL mode (must be @c DYAD_DTL_MARGO). Redundant
 *                      since @c dyad_dtl_init() already stores the mode
 *                      in @c ctx->dtl_handle->mode before dispatch
 *                      (see TODO).
 * @param[in] comm_mode Communication direction. @c DYAD_COMM_SEND
 *                      initializes Margo as a client (producer side);
 *                      @c DYAD_COMM_RECV initializes Margo as a server
 *                      (consumer side).
 * @param[in] debug     If @c true, enables verbose debug logging.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK             Initialization succeeded.
 * @retval DYAD_RC_SYSFAIL        Failed to allocate the @c dyad_dtl_margo
 *                                struct.
 * @retval DYAD_RC_MARGO_BAD_PROTO The configured Mercury/NA protocol is
 *                                not available on this system.
 * @retval DYAD_RC_MARGOINIT_FAIL @c margo_init() failed, or any other
 *                                error occurred during initialization.
 *
 * @todo Remove @p mode parameter — it is already stored in
 *       @c ctx->dtl_handle->mode by @c dyad_dtl_init() before this
 *       function is called.
 */
dyad_rc_t dyad_dtl_margo_init (const dyad_ctx_t *ctx,
                               dyad_dtl_mode_t mode,
                               dyad_dtl_comm_mode_t comm_mode,
                               bool debug);

/**
 * @brief Packs a file fetch request into a JSON object for a Margo RPC call.
 *
 * @details
 * Creates a Jansson JSON object containing the fields needed by the
 * producer to locate the file and establish a Margo connection back
 * to the consumer:
 *
 * - @c "upath"    — relative path of the file to fetch.
 * - @c "tag_prod" — Flux rank of the producer broker, used to route
 *                   the Flux RPC to the correct broker.
 * - @c "pid_cons" — process ID of the consumer, used by the producer
 *                   to identify the requesting process.
 * - @c "addr"     — Margo address string of the consumer's Margo server,
 *                   obtained via @c margo_addr_to_string(). The producer
 *                   uses this address to establish an RDMA connection back
 *                   to the consumer to push the file data.
 *
 * Unlike the Flux RPC backend which only packs @c upath, the Margo
 * backend must also include the consumer's Margo server address since
 * the data transfer is initiated by the producer connecting back to
 * the consumer (RDMA push model), rather than the consumer pulling
 * from the producer.
 *
 * The caller is responsible for decrementing the reference count of
 * @p packed_obj via @c json_decref() when it is no longer needed.
 *
 * @param[in]  ctx           DYAD context.
 * @param[in]  upath         Relative path of the file to fetch.
 * @param[in]  producer_rank Flux rank of the producer broker.
 * @param[out] packed_obj    Set to the allocated JSON object on success.
 *                           Undefined on failure.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK      The JSON object was created successfully.
 * @retval DYAD_RC_BADPACK @c json_pack() failed to create the object.
 */
dyad_rc_t dyad_dtl_margo_rpc_pack (const dyad_ctx_t *ctx,
                                   const char *upath,
                                   uint32_t producer_rank,
                                   json_t **packed_obj);

/**
 * @brief Unpacks a file fetch request from an incoming Flux RPC message
 *        and resolves the consumer's Margo address.
 *
 * @details
 * Extracts the following fields from the JSON payload of @p msg using
 * @c flux_request_unpack():
 *
 * - @c "upath"    — relative path of the requested file.
 * - @c "tag_prod" — Flux rank of the producer broker (extracted but
 *                   not used on the producer side).
 * - @c "pid_cons" — process ID of the consumer (extracted but not
 *                   used on the producer side).
 * - @c "addr"     — Margo address string of the consumer's Margo server.
 *
 * After successful unpacking, resolves the consumer's Margo address
 * string to a @c hg_addr_t via @c margo_addr_lookup() and stores it
 * in @c margo_handle->remote_addr. This address is used by
 * @c dyad_dtl_margo_send() to establish the RDMA connection back to
 * the consumer.
 *
 * @note @p upath is owned by the Flux message @p msg and must not be
 *       freed by the caller. It remains valid only for the lifetime
 *       of @p msg.
 *
 * @note @c tag_prod and @c pid_cons are unpacked for protocol
 *       completeness but are not used on the producer side. They may
 *       be useful for debugging or future extensions.
 *
 * @param[in]  ctx   DYAD context.
 * @param[in]  msg   Incoming Flux RPC message containing the JSON payload
 *                   packed by @c dyad_dtl_margo_rpc_pack().
 * @param[out] upath Set to the relative path of the requested file,
 *                   extracted from the message payload. Valid for the
 *                   lifetime of @p msg.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK        Unpacking and address resolution succeeded.
 * @retval DYAD_RC_BADUNPACK @c flux_request_unpack() failed to extract
 *                           the required fields from the message.
 *
 * @note Unlike the Flux RPC backend, the Margo backend does not store
 *       @p msg in the DTL handle after unpacking. In the Flux RPC
 *       backend, @p msg is stored because it is reused across multiple
 *       @c flux_respond_raw() calls on the same streaming RPC. This
 *       backend does not use streaming RPC — @p msg is only needed
 *       to extract the initial request fields, and the data transfer
 *       proceeds over a separate Margo RDMA channel independently of
 *       the Flux message.
 */
dyad_rc_t dyad_dtl_margo_rpc_unpack (const dyad_ctx_t *ctx, const flux_msg_t *msg, char **upath);

/**
 * @brief Sends the initial RPC acknowledgement from the service to the consumer.
 *
 * @details
 * No-op for the Margo DTL. Unlike the Flux RPC backend, the Margo
 * data transfer is driven by the producer connecting back to the
 * consumer's Margo server directly via RDMA, so no explicit
 * acknowledgement over the Flux RPC channel is needed before data
 * transfer begins.
 *
 * @param[in] ctx      DYAD context.
 * @param[in] orig_msg Unused by this backend.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_margo_rpc_respond (const dyad_ctx_t *ctx, const flux_msg_t *orig_msg);

/**
 * @brief Receives the initial RPC response from the service.
 *
 * @details
 * No-op for the Margo DTL. The consumer does not need to process a
 * Flux RPC response before data transfer begins — it waits directly
 * on @c margo_handle->recv_ready, which is set by @c data_ready_rpc()
 * after the RDMA pull completes.
 *
 * @param[in] ctx Unused by this backend.
 * @param[in] f   Unused by this backend.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_margo_rpc_recv_response (const dyad_ctx_t *ctx, flux_future_t *f);

/**
 * @brief Allocates a buffer for Margo DTL data transfer.
 *
 * @details
 * Allocates a buffer of @p data_size bytes via @c malloc(). The function
 * validates @p data_buf before allocation:
 * - If @p data_buf is @c NULL, the caller passed an invalid output
 *   pointer and @c DYAD_RC_BADBUF is returned.
 * - If @p *data_buf is non-@c NULL, a buffer is already present and
 *   overwriting it would cause a memory leak, so @c DYAD_RC_BADBUF
 *   is returned.
 *
 * The allocated buffer must be released via
 * @c dyad_dtl_margo_return_buffer().
 *
 * @note A @c posix_memalign() path exists in the source but is disabled
 *       (@c #else branch, @c #if 1 selects @c malloc()). Unlike the Flux
 *       RPC DTL which uses page-aligned allocation, the Margo DTL
 *       currently uses plain @c malloc() since Margo manages its own
 *       RDMA memory registration separately via @c margo_bulk_create().
 *
 * @param[in]  ctx       DYAD context.
 * @param[in]  data_size Number of bytes to allocate.
 * @param[out] data_buf  Must point to a @c NULL pointer on entry. Set
 *                       to the allocated buffer on success.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK      Buffer allocated successfully.
 * @retval DYAD_RC_BADBUF  @p data_buf is @c NULL or @p *data_buf is
 *                         already non-@c NULL.
 * @retval DYAD_RC_SYSFAIL @c malloc() failed to allocate the buffer.
 */
dyad_rc_t dyad_dtl_margo_get_buffer (const dyad_ctx_t *ctx, size_t data_size, void **data_buf);

/**
 * @brief Releases a buffer previously allocated by
 *        @c dyad_dtl_margo_get_buffer().
 *
 * @details
 * Frees the buffer pointed to by @p *data_buf. The function validates
 * @p data_buf before freeing:
 * - If @p data_buf is @c NULL, the caller passed an invalid pointer
 *   and @c DYAD_RC_BADBUF is returned.
 * - If @p *data_buf is @c NULL, the buffer has already been freed or
 *   was never allocated, and @c DYAD_RC_BADBUF is returned.
 *
 * @note @p *data_buf is not set to @c NULL after freeing. The caller
 *       is responsible for nulling the pointer to prevent use-after-free
 *       (see TODO).
 *
 * @todo Set @p *data_buf to @c NULL after @c free() to prevent
 *       use-after-free:
 *       @code
 *       free (*data_buf);
 *       *data_buf = NULL;
 *       @endcode
 *
 * @param[in]     ctx      DYAD context.
 * @param[in,out] data_buf Pointer to the buffer to free. @p *data_buf
 *                         must be non-@c NULL on entry.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK     Buffer freed successfully.
 * @retval DYAD_RC_BADBUF @p data_buf is @c NULL or @p *data_buf is
 *                        @c NULL.
 */
dyad_rc_t dyad_dtl_margo_return_buffer (const dyad_ctx_t *ctx, void **data_buf);

/**
 * @brief Establishes the Margo DTL data channel.
 *
 * @details
 * No-op for the Margo DTL. The consumer's Margo address is resolved
 * to an @c hg_addr_t during @c dyad_dtl_margo_rpc_unpack() and stored
 * in @c margo_handle->remote_addr. The producer connects directly to
 * this address in @c dyad_dtl_margo_send() via @c margo_create() and
 * @c margo_forward() without a separate connection setup step.
 *
 * Endpoint caching is not needed since each producer-consumer pair
 * performs a single data transfer per file fetch — the connection is
 * created, used, and destroyed within a single @c dyad_dtl_margo_send()
 * call.
 *
 * @param[in] ctx DYAD context.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_margo_establish_connection (const dyad_ctx_t *ctx);

/**
 * @brief Sends file data to the consumer via Margo RDMA.
 *
 * @details
 * Registers @p buf as a read-only Mercury bulk handle via
 * @c margo_bulk_create(), then sends an RPC to the consumer's Margo
 * server at @c margo_handle->remote_addr (resolved during
 * @c dyad_dtl_margo_rpc_unpack()) via @c margo_forward(). The RPC
 * payload contains the bulk handle and the buffer size, allowing the
 * consumer's @c data_ready_rpc() handler to perform an RDMA pull
 * from the producer's registered buffer.
 *
 * @c margo_forward() blocks until the consumer responds, confirming
 * that the RDMA pull is complete. The producer then frees the RPC
 * output and destroys the handle.
 *
 * @note Unlike the Flux RPC backend where the producer needs the original
 *       request message (@c flux_msg_t) as a reply address to route
 *       @c flux_respond_raw() back to the correct consumer, this Margo-based
 *       backend uses an RDMA pull model. During @c dyad_dtl_margo_init(),
 *       the consumer initializes its Margo instance in
 *       @c MARGO_SERVER_MODE so that the producer can connect back to
 *       it. The consumer then embeds its own Margo address as a string
 *       in the Flux RPC request payload via @c dyad_dtl_margo_rpc_pack().
 *       The producer extracts and resolves this address to an
 *       @c hg_addr_t via @c margo_addr_lookup() during
 *       @c dyad_dtl_margo_rpc_unpack(), storing it in
 *       @c margo_handle->remote_addr. This address is used here to
 *       connect back to the consumer and trigger the RDMA pull,
 *       without needing the original @c flux_msg_t.
 *
 * @param[in] ctx    DYAD context. @c margo_handle->remote_addr must
 *                   already be set by @c dyad_dtl_margo_rpc_unpack().
 * @param[in] buf    Buffer containing the file data to send.
 * @param[in] buflen Number of bytes in @p buf.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK Always returned. Error handling for Margo calls
 *                    is not yet implemented (see TODO).
 *
 * @todo Add error checking for @c margo_bulk_create(),
 *       @c margo_create(), @c margo_forward(), and
 *       @c margo_get_output() — all currently unchecked.
 */
dyad_rc_t dyad_dtl_margo_send (const dyad_ctx_t *ctx, void *buf, size_t buflen);

/**
 * @brief Receives file data from the producer via the Margo DTL.
 *
 * @details
 * Busy-waits on @c margo_handle->recv_ready, sleeping 100 microseconds
 * between checks, until @c data_ready_rpc() sets it to 1 after
 * completing the RDMA pull from the producer. Once the data is ready,
 * allocates a new buffer, copies the received data from
 * @c margo_handle->recv_buffer, and frees the buffer allocated by
 * @c data_ready_rpc().
 *
 * After copying, the Margo handle state is reset for the next transfer:
 * - @c recv_buffer is freed and set to @c NULL.
 * - @c recv_len is reset to 0.
 * - @c recv_ready is reset to 0.
 *
 * @note The data flow for Margo receive is inverted compared to the
 *       Flux RPC backend. In the Flux RPC backend the producer pushes
 *       data by calling @c flux_respond_raw(), and the consumer reads
 *       it via @c flux_rpc_get_raw(). In this Margo-based backend the producer
 *       registers its buffer and notifies the consumer's Margo server,
 *       which performs an RDMA pull into @c margo_handle->recv_buffer
 *       via @c data_ready_rpc(). The consumer then copies from that
 *       buffer here. The actual data movement therefore happens in
 *       @c data_ready_rpc() running on the progress loop ES, not in
 *       this function.
 *
 * @note The busy-wait with @c usleep(100) is a simple polling approach.
 *       A condition variable or Argobots synchronization primitive would
 *       be more efficient but would require additional coordination with
 *       the Margo progress loop ES (see TODO).
 *
 * @todo Replace the busy-wait loop with a more efficient synchronization
 *       mechanism such as an Argobots eventual or condition variable.
 *
 * @param[in]  ctx    DYAD context.
 * @param[out] buf    Set to a newly allocated buffer containing a copy
 *                    of the received file data. The caller must free
 *                    this buffer via @c ctx->dtl_handle->return_buffer().
 * @param[out] buflen Set to the number of bytes received.
 *
 * @return Always returns @c DYAD_RC_OK. Error handling for allocation
 *         failure and copy errors is not yet implemented (see TODO).
 *
 * @todo Add error checking for @c malloc() failure.
 */
dyad_rc_t dyad_dtl_margo_recv (const dyad_ctx_t *ctx, void **buf, size_t *buflen);

/**
 * @brief Closes the Margo DTL data channel.
 *
 * @details
 * No-op for the Margo DTL. The RPC handle created in
 * @c dyad_dtl_margo_send() is destroyed within that function via
 * @c margo_destroy() immediately after the transfer completes.
 * No persistent connection state is maintained between transfers
 * since endpoint caching is not needed for DYAD's single-transfer
 * per file fetch usage pattern.
 *
 * @param[in] ctx DYAD context.
 *
 * @return Always returns @c DYAD_RC_OK.
 *
 * @todo The original implementation intended to perform different
 *       teardown depending on @c comm_mode (producer vs consumer).
 *       This was never implemented and the code was commented out.
 *       Revisit whether comm_mode-specific teardown is needed, for
 *       example to free @c margo_handle->remote_addr on the producer
 *       side via @c margo_addr_free().
 */
dyad_rc_t dyad_dtl_margo_close_connection (const dyad_ctx_t *ctx);

/**
 * @brief Finalizes and frees the Margo DTL internal state.
 *
 * @details
 * Releases all resources associated with the Margo DTL in the following
 * order:
 *
 *  1. If @c margo_handle->mid is a valid Margo instance, frees the
 *     local Margo address via @c margo_addr_free().
 *  2. If @c margo_handle->remote_addr is non-@c NULL, frees the remote
 *     address (the consumer's resolved Margo server address) via
 *     @c margo_addr_free().
 *  3. Finalizes the Margo instance via @c margo_finalize(), which shuts
 *     down the Mercury progress loop and any associated Argobots ESs.
 *  4. Frees the @c dyad_dtl_margo struct and sets the handle pointer
 *     to @c NULL.
 *
 * If @c ctx->dtl_handle is @c NULL or
 * @c ctx->dtl_handle->private_dtl.margo_dtl_handle is @c NULL, the
 * function is a no-op and returns @c DYAD_RC_OK. This allows
 * @c dyad_dtl_margo_finalize() to be called safely on a partially
 * initialized or already-finalized context, for example when
 * @c dyad_dtl_margo_init() fails partway through and calls this
 * function to clean up.
 *
 * @note This function only frees the @c dyad_dtl_margo struct. The
 *       outer @c dyad_dtl handle is freed by @c dyad_dtl_finalize(),
 *       which calls this function as part of the full teardown sequence.
 *
 * @param[in] ctx DYAD context. On return,
 *                @c ctx->dtl_handle->private_dtl.margo_dtl_handle
 *                is @c NULL.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_margo_finalize (const dyad_ctx_t *ctx);

#endif /* DYAD_DTL_margo_H */
