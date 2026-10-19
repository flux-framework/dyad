#ifndef DYAD_DTL_FLUX_H
#define DYAD_DTL_FLUX_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <stdlib.h>

#include <dyad/dtl/dyad_dtl_api.h>

struct dyad_dtl_flux {
    flux_t *h;
    dyad_dtl_comm_mode_t comm_mode;
    bool debug;
    flux_future_t *f;
    flux_msg_t *msg;
};

typedef struct dyad_dtl_flux dyad_dtl_flux_t;

/**
 * @brief Per-request state detached from the shared @c dyad_dtl_flux
 *        fields by @c dyad_dtl_flux_detach_request(), so a request can be
 *        finished later (e.g. from a worker-thread completion queue)
 *        without racing a later request's unpack call.
 */
struct dyad_dtl_flux_req_state {
    flux_t *h;
    flux_msg_t *msg;  // incref'd; released by dyad_dtl_flux_send_detached()
};

/**
 * @brief Initializes the Flux RPC DTL internal state.
 *
 * @details
 * Allocates and populates the @c dyad_dtl_flux internal state struct,
 * then wires all function pointers in @c ctx->dtl_handle to their
 * Flux RPC implementations:
 *
 * - @c rpc_pack             ->@c dyad_dtl_flux_rpc_pack
 * - @c rpc_unpack           -> @c dyad_dtl_flux_rpc_unpack
 * - @c rpc_respond          -> @c dyad_dtl_flux_rpc_respond
 * - @c rpc_recv_response    -> @c dyad_dtl_flux_rpc_recv_response
 * - @c get_buffer           -> @c dyad_dtl_flux_get_buffer
 * - @c return_buffer        -> @c dyad_dtl_flux_return_buffer
 * - @c establish_connection -> @c dyad_dtl_flux_establish_connection
 * - @c send                 -> @c dyad_dtl_flux_send
 * - @c recv                 -> @c dyad_dtl_flux_recv
 * - @c close_connection     -> @c dyad_dtl_flux_close_connection
 *
 * The Flux handle is borrowed from @c ctx->h and stored as a non-owning
 * pointer — it is not closed by @c dyad_dtl_flux_finalize(). The pending
 * future (@c f) and message (@c msg) fields are initialized to @c NULL
 * and are set during RPC operations.
 *
 * @param[in] ctx       DYAD context. @c ctx->dtl_handle must already be
 *                      allocated by @c dyad_dtl_init().
 * @param[in] mode      DTL mode (must be @c DYAD_DTL_FLUX_RPC. see TODO).
 * @param[in] comm_mode Communication direction (@c DYAD_COMM_SEND for
 *                      producer, @c DYAD_COMM_RECV for consumer).
 * @param[in] debug     If @c true, enables verbose debug logging.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK      Initialization succeeded.
 * @retval DYAD_RC_SYSFAIL Failed to allocate the @c dyad_dtl_flux struct.
 *
 * @todo Remove @p mode parameter — it is already stored in
 *       @c ctx->dtl_handle->mode by @c dyad_dtl_init() before this
 *       function is called.
 */
dyad_rc_t dyad_dtl_flux_init (const dyad_ctx_t *ctx,
                              dyad_dtl_mode_t mode,
                              dyad_dtl_comm_mode_t comm_mode,
                              bool debug);

/**
 * @brief Packs a file fetch request into a JSON object for a Flux RPC call.
 *
 * @details
 * Creates a Jansson JSON object of the form @c {"upath": "<upath>"}
 * using @c json_pack(). The @p producer_rank parameter is accepted for
 * interface consistency with other DTL backends but is not included in
 * the packed object — the Flux RPC is routed to the producer broker by
 * rank at the Flux level rather than embedded in the payload.
 *
 * The caller is responsible for decrementing the reference count of
 * @p packed_obj via @c json_decref() when it is no longer needed.
 *
 * @param[in]  ctx           DYAD context.
 * @param[in]  upath         Relative path of the file to fetch.
 * @param[in]  producer_rank Flux rank of the producer broker. Not
 *                           embedded in the payload for Flux RPC.
 * @param[out] packed_obj    Set to the allocated JSON object on success.
 *                           Undefined on failure.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK      The JSON object was created successfully.
 * @retval DYAD_RC_BADPACK @c json_pack() failed to create the object.
 */
dyad_rc_t dyad_dtl_flux_rpc_pack (const dyad_ctx_t *ctx,
                                  const char *DYAD_RESTRICT upath,
                                  uint32_t producer_rank,
                                  json_t **DYAD_RESTRICT packed_obj);

/**
 * @brief Unpacks a file fetch request from an incoming Flux RPC message.
 *
 * @details
 * Extracts the @c upath field from the JSON payload of @p msg using
 * @c flux_request_unpack(). On success, stores a non-owning pointer to
 * @p msg in the Flux DTL handle so that @c dyad_dtl_flux_rpc_respond()
 * can respond to the same message later without the caller needing to
 * pass it again.
 *
 * @note The @p upath string is owned by the Flux message @p msg and
 *       must not be freed by the caller. It remains valid only for the
 *       lifetime of @p msg.
 *
 * @note The stored @p msg pointer is non-owning — the Flux DTL does not
 *       free or destroy the message.
 *
 * @param[in]  ctx   DYAD context.
 * @param[in]  msg   Incoming Flux RPC message containing the JSON payload.
 * @param[out] upath Set to the relative path of the requested file,
 *                   extracted from the message payload. Valid for the
 *                   lifetime of @p msg.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK        Unpacking succeeded and @p upath is set.
 * @retval DYAD_RC_BADUNPACK @c flux_request_unpack() failed to extract
 *                           the @c upath field from the message.
 *
 * @note @p msg is stored as a non-owning pointer in the Flux DTL handle
 *       for use by @c dyad_dtl_flux_send(), which calls
 *       @c flux_respond_raw() with the stored message to route the
 *       response back to the correct consumer. In the Flux streaming
 *       RPC protocol, the original request message serves as the reply
 *       address — without it the producer cannot direct the response to
 *       the consumer that issued the request. The message is owned by
 *       the Flux broker and must not be freed by DYAD.
 */
dyad_rc_t dyad_dtl_flux_rpc_unpack (const dyad_ctx_t *ctx, const flux_msg_t *msg, char **upath);

/**
 * @brief Packs a byte-range fetch request into a JSON object for a Flux RPC call.
 *
 * @details
 * Same as @c dyad_dtl_flux_rpc_pack() but additionally packs @p offset and
 * @p length, producing @c {"upath": "<upath>", "offset": <offset>, "length": <length>}.
 *
 * @param[in]  ctx           DYAD context.
 * @param[in]  upath         Relative path of the file to fetch from.
 * @param[in]  producer_rank Flux rank of the producer broker. Not embedded
 *                           in the payload for Flux RPC.
 * @param[in]  offset        Starting byte offset of the requested range.
 * @param[in]  length        Number of bytes requested.
 * @param[out] packed_obj    Set to the allocated JSON object on success.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK      The JSON object was created successfully.
 * @retval DYAD_RC_BADPACK @c json_pack() failed to create the object.
 */
dyad_rc_t dyad_dtl_flux_rpc_pack_range (const dyad_ctx_t *ctx,
                                        const char *DYAD_RESTRICT upath,
                                        uint32_t producer_rank,
                                        size_t offset,
                                        size_t length,
                                        json_t **DYAD_RESTRICT packed_obj);

/**
 * @brief Unpacks a byte-range fetch request from an incoming Flux RPC message.
 *
 * @details
 * Same as @c dyad_dtl_flux_rpc_unpack() but additionally extracts @p offset
 * and @p length from the JSON payload.
 *
 * @param[in]  ctx    DYAD context.
 * @param[in]  msg    Incoming Flux RPC message containing the JSON payload.
 * @param[out] upath  Relative path of the requested file. Valid for the
 *                    lifetime of @p msg.
 * @param[out] offset Starting byte offset of the requested range.
 * @param[out] length Number of bytes requested.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK        Unpacking succeeded.
 * @retval DYAD_RC_BADUNPACK @c flux_request_unpack() failed to extract the
 *                           fields from the message.
 */
dyad_rc_t dyad_dtl_flux_rpc_unpack_range (const dyad_ctx_t *ctx,
                                          const flux_msg_t *msg,
                                          char **upath,
                                          size_t *offset,
                                          size_t *length);

/**
 * @brief Sends the initial RPC acknowledgement from the service to the consumer.
 *
 * @details
 * For the Flux RPC DTL backend this is a no-op — the Flux streaming RPC
 * protocol does not require an explicit acknowledgement before data
 * transfer begins. The function exists to satisfy the @c rpc_respond
 * function pointer in @c dyad_dtl and to maintain a consistent interface
 * across all backends.
 *
 * @param[in] ctx      DYAD context.
 * @param[in] orig_msg The incoming Flux RPC message to respond to.
 *                     Unused by this backend.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_flux_rpc_respond (const dyad_ctx_t *ctx, const flux_msg_t *orig_msg);

/**
 * @brief Receives the initial RPC response from the service and stores
 *        the Flux future for subsequent data transfer.
 *
 * @details
 * Stores @p f in the Flux DTL handle so that subsequent calls to
 * @c dyad_dtl_flux_recv() can retrieve data from the same streaming
 * RPC future without the caller needing to pass it again.
 *
 * For the Flux RPC backend, no blocking wait is performed here — the
 * future is stored and consumed lazily by @c dyad_dtl_flux_recv().
 *
 * @param[in] ctx DYAD context.
 * @param[in] f   Flux future representing the pending streaming RPC
 *                response. Stored as a non-owning pointer; the caller
 *                retains ownership and must destroy it after the
 *                transfer is complete.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_flux_rpc_recv_response (const dyad_ctx_t *ctx, flux_future_t *f);

/**
 * @brief Allocates a page-aligned buffer for Flux RPC data transfer.
 *
 * @details
 * Allocates a buffer of @p data_size bytes aligned to the system page
 * size via @c posix_memalign(). Page alignment is required for
 * compatibility with direct I/O and RDMA-capable transports that may
 * be used alongside the Flux RPC backend.
 *
 * The function validates @p data_buf before allocation:
 * - If @p data_buf is @c NULL, the caller passed an invalid output
 *   pointer and @c DYAD_RC_BADBUF is returned.
 * - If @p *data_buf is non-@c NULL, a buffer is already present and
 *   overwriting it would cause a memory leak, so @c DYAD_RC_BADBUF
 *   is returned.
 *
 * The allocated buffer must be released via @c dyad_dtl_flux_return_buffer().
 *
 * @note A plain @c malloc() path exists in the source but is disabled
 *       (@c #if 0). The active path always uses @c posix_memalign().
 *
 * @param[in]  ctx       DYAD context.
 * @param[in]  data_size Number of bytes to allocate.
 * @param[out] data_buf  Must point to a @c NULL pointer on entry. Set
 *                       to the allocated page-aligned buffer on success.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK      Buffer allocated successfully.
 * @retval DYAD_RC_BADBUF  @p data_buf is @c NULL or @p *data_buf is
 *                         already non-@c NULL.
 * @retval DYAD_RC_SYSFAIL @c posix_memalign() failed.
 */
dyad_rc_t dyad_dtl_flux_get_buffer (const dyad_ctx_t *ctx, size_t data_size, void **data_buf);

/**
 * @brief Releases a buffer previously allocated by @c dyad_dtl_flux_get_buffer().
 *
 * @details
 * Frees the buffer pointed to by @p *data_buf. The function validates
 * @p data_buf before freeing:
 * - If @p data_buf is @c NULL, the caller passed an invalid pointer
 *   and @c DYAD_RC_BADBUF is returned.
 * - If @p *data_buf is @c NULL, the buffer has already been freed or
 *   was never allocated, and @c DYAD_RC_BADBUF is returned.
 *
 * @note The caller is responsible for setting @p *data_buf to @c NULL
 *       after this call. Unlike some other buffer management functions
 *       in DYAD, this function does not null the pointer on return.
 *
 * @param[in]     ctx      DYAD context.
 * @param[in,out] data_buf Pointer to the buffer to free. @p *data_buf
 *                         must be non-@c NULL on entry.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK     Buffer freed successfully.
 * @retval DYAD_RC_BADBUF @p data_buf is @c NULL or @p *data_buf is
 *                        @c NULL.
 * @todo Set @p *data_buf to @c NULL after @c free() to prevent
 *       use-after-free. This is a one-line fix:
 *       @code
 *       free (*data_buf);
 *       *data_buf = NULL;
 *       @endcode
 */
dyad_rc_t dyad_dtl_flux_return_buffer (const dyad_ctx_t *ctx, void **data_buf);

/**
 * @brief Establishes the DTL data channel for the Flux RPC backend.
 *
 * @details
 * For the Flux RPC backend this is a no-op — data is transferred
 * directly over the existing Flux streaming RPC connection established
 * during @c rpc_pack() / @c rpc_recv_response(), so no additional
 * connection setup is required. The function exists to satisfy the
 * @c establish_connection function pointer in @c dyad_dtl and to
 * maintain a consistent interface across all backends.
 *
 * @param[in] ctx DYAD context.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_flux_establish_connection (const dyad_ctx_t *ctx);

/**
 * @brief Sends file data to the consumer via a Flux RPC response.
 *
 * @details
 * Sends @p buf as the raw payload of a Flux RPC response using
 * @c flux_respond_raw(). The response is addressed to the consumer
 * using the message stored in the Flux DTL handle by
 * @c dyad_dtl_flux_rpc_unpack(). This is a streaming RPC response —
 * the consumer receives it via @c dyad_dtl_flux_recv() which reads
 * successive responses from the same Flux future until @c ENODATA
 * signals end-of-stream.
 *
 * @param[in] ctx    DYAD context. The Flux handle and the stored
 *                   request message are read from the Flux DTL handle.
 * @param[in] buf    Buffer containing the file data to send.
 * @param[in] buflen Number of bytes in @p buf.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK       Data sent successfully.
 * @retval DYAD_RC_FLUXFAIL @c flux_respond_raw() failed.
 */
dyad_rc_t dyad_dtl_flux_send (const dyad_ctx_t *ctx, void *buf, size_t buflen);

/**
 * @brief Receives file data from the producer via a Flux streaming RPC.
 *
 * @details
 * Retrieves the raw payload of the next Flux RPC response from the
 * future stored in the Flux DTL handle by
 * @c dyad_dtl_flux_rpc_recv_response(). The received data is copied
 * into a freshly allocated buffer obtained via
 * @c ctx->dtl_handle->get_buffer().
 *
 * After retrieval, @c flux_future_reset() is called on the stored
 * future regardless of success or failure, allowing the future to be
 * reused for subsequent streaming responses in the same RPC session.
 *
 * Error handling:
 * - If the stored future is @c NULL, the function returns
 *   @c DYAD_RC_FLUXFAIL immediately.
 * - If @c flux_rpc_get_raw() fails with @c errno == @c ENODATA, the
 *   producer has signalled end-of-stream and @c DYAD_RC_RPC_FINISHED
 *   is returned. The caller should treat this as a normal termination
 *   condition rather than a hard error.
 * - If @c flux_rpc_get_raw() fails for any other reason,
 *   @c DYAD_RC_BADRPC is returned.
 * - If buffer allocation fails, @p *buf is set to @c NULL and
 *   @p *buflen is set to 0.
 *
 * @param[in]  ctx    DYAD context. The stored Flux future is read from
 *                    the Flux DTL handle.
 * @param[out] buf    Set to a newly allocated buffer containing the
 *                    received file data on success. The caller must
 *                    release it via @c ctx->dtl_handle->return_buffer().
 *                    Set to @c NULL on failure.
 * @param[out] buflen Set to the number of bytes received on success.
 *                    Set to 0 on failure.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK            Data received and copied successfully.
 * @retval DYAD_RC_FLUXFAIL      The stored Flux future is @c NULL.
 * @retval DYAD_RC_RPC_FINISHED  @c flux_rpc_get_raw() returned
 *                               @c ENODATA, signalling end-of-stream.
 * @retval DYAD_RC_BADRPC        @c flux_rpc_get_raw() failed for a
 *                               reason other than @c ENODATA.
 * @retval other                 Any error code returned by
 *                               @c get_buffer() on allocation failure.
 */
dyad_rc_t dyad_dtl_flux_recv (const dyad_ctx_t *ctx, void **buf, size_t *buflen);

/**
 * @brief Closes the Flux RPC DTL data channel.
 *
 * @details
 * Clears the stored Flux future (@c f) and request message (@c msg)
 * pointers in the Flux DTL handle by setting them to @c NULL. Neither
 * pointer is destroyed or freed here — both are non-owning references:
 *
 * - The Flux future (@c f) is owned by the caller that passed it to
 *   @c dyad_dtl_flux_rpc_recv_response() and must be destroyed by
 *   that caller via @c flux_future_destroy().
 * - The request message (@c msg) is owned by the Flux broker and
 *   must not be freed by DYAD.
 *
 * After this call the DTL handle is ready to be reused for a new
 * RPC session by calling @c rpc_pack() / @c rpc_unpack() again.
 *
 * @param[in] ctx DYAD context.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_flux_close_connection (const dyad_ctx_t *ctx);

/**
 * @brief Detaches the current request's message from the shared,
 *        single-slot @c msg field into an independently-owned request
 *        state blob.
 *
 * @details
 * Takes a reference on the message stored by the preceding
 * @c dyad_dtl_flux_rpc_unpack_range() call via @c flux_msg_incref() (the
 * standard Flux idiom for retaining a message beyond the synchronous
 * callback scope that received it) and stores it in a newly allocated
 * @c struct dyad_dtl_flux_req_state, then clears the shared @c msg field
 * so a subsequent request's unpack call cannot be confused with this one.
 *
 * @param[in]  ctx       DYAD context.
 * @param[out] req_state Set to a newly allocated request-state blob owning
 *                       an incref'd reference to the request message. Must
 *                       be passed to @c dyad_dtl_flux_send_detached()
 *                       exactly once.
 *
 * @return Always returns @c DYAD_RC_OK, unless allocation fails
 *         (@c DYAD_RC_SYSFAIL).
 */
dyad_rc_t dyad_dtl_flux_detach_request (const dyad_ctx_t *ctx, void **req_state);

/**
 * @brief Sends file data to the consumer using a previously detached
 *        request's message, instead of the shared @c msg field.
 *
 * @details
 * Equivalent to @c dyad_dtl_flux_send() but reads the request message from
 * @p req_state (as produced by @c dyad_dtl_flux_detach_request()) instead
 * of the shared, single-slot field -- safe to call after other requests'
 * unpack calls have since overwritten that shared field. Releases the
 * message reference via @c flux_msg_decref() and frees @p req_state before
 * returning, regardless of success or failure.
 *
 * @note Must be called from the module's reactor thread -- the @c flux_t
 *       handle used internally is not safe to touch from any other thread.
 *       @see dyad_dtl::send_detached_is_thread_safe (false for this
 *       backend).
 *
 * @param[in] ctx       DYAD context.
 * @param[in] req_state Request state from a prior
 *                      @c dyad_dtl_flux_detach_request() call. Freed by
 *                      this call.
 * @param[in] buf       Buffer containing the file data to send.
 * @param[in] buflen    Number of bytes in @p buf.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK       Data sent successfully.
 * @retval DYAD_RC_FLUXFAIL @c flux_respond_raw() failed.
 */
dyad_rc_t dyad_dtl_flux_send_detached (const dyad_ctx_t *ctx,
                                       void *req_state,
                                       void *buf,
                                       size_t buflen);

/**
 * @brief Frees a detached request's message reference without sending
 *        data, for use on an I/O-error path.
 *
 * @param[in] ctx       DYAD context.
 * @param[in] req_state Request state from a prior
 *                      @c dyad_dtl_flux_detach_request() call. Freed by
 *                      this call.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_flux_abort_detached (const dyad_ctx_t *ctx, void *req_state);

/**
 * @brief Finalizes and frees the Flux RPC DTL internal state.
 *
 * @details
 * Clears all pointers in the Flux DTL handle, frees the
 * @c dyad_dtl_flux struct, and sets the handle pointer to @c NULL.
 * If @c ctx->dtl_handle is already @c NULL the function is a no-op.
 *
 * The three stored pointers are cleared as follows before freeing:
 * - @c h   — set to @c NULL. The Flux handle is non-owning; it is
 *             borrowed from @c ctx->h and must not be closed here.
 * - @c f   — set to @c NULL. The Flux future is non-owning; the
 *             caller that created it is responsible for destroying
 *             it via @c flux_future_destroy().
 * - @c msg — set to @c NULL. The request message is non-owning; it
 *             is owned by the Flux broker and must not be freed here.
 *
 * @note This function only frees the @c dyad_dtl_flux struct. The
 *       outer @c dyad_dtl handle is freed by @c dyad_dtl_finalize(),
 *       which calls this function as part of the full teardown sequence.
 *
 * @param[in] ctx DYAD context. @c ctx->dtl_handle->private_dtl.flux_dtl_handle
 *                is set to @c NULL on return.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_dtl_flux_finalize (const dyad_ctx_t *ctx);

#endif /* DYAD_DTL_FLUX_H */
