#ifndef DYAD_DTL_DYAD_DTL_IMPL_H
#define DYAD_DTL_DYAD_DTL_IMPL_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

// clang-format off
#include <jansson.h>
#include <sys/types.h>
#include <dyad/common/dyad_structures_int.h>
#include <dyad/common/dyad_dtl.h>
#include <dyad/common/dyad_rc.h>
#include <flux/core.h>
// clang-format on

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#include <dyad/common/dyad_dtl.h>
#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures.h>

/** Internal @c send() length denoting a producer-side transfer failure. */
#define DYAD_DTL_SEND_ERROR SIZE_MAX

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal state for the UCX DTL backend.
 * @see dyad_dtl_private_t
 */
struct dyad_dtl_ucx;

/**
 * @brief Internal state for the Flux RPC DTL backend.
 * @see dyad_dtl_private_t
 */
struct dyad_dtl_flux;

/**
 * @brief Union holding a pointer to the internal state of the active
 *        DTL backend.
 *
 * @details
 * Only one member is valid at a time, selected by the @c mode field of
 * the owning @c dyad_dtl struct. Each member points to a struct that
 * holds all state needed by that transport backend — for example, UCX
 * endpoint handles and memory registrations, or Flux RPC futures and
 * buffers. The union is aligned to 16 bytes to satisfy the alignment
 * requirements of the underlying backend structs.
 */
union dyad_dtl_private {
    struct dyad_dtl_ucx *ucx_dtl_handle;
    struct dyad_dtl_flux *flux_dtl_handle;
    struct dyad_dtl_margo *margo_dtl_handle;
} __attribute__ ((aligned (16)));
typedef union dyad_dtl_private dyad_dtl_private_t;

/**
 * @brief Data Transport Layer handle.
 *
 * @details
 * Provides a uniform interface for all DTL backends through a table of
 * function pointers. The active backend is selected at initialization
 * time by @c dyad_dtl_init() and stored in @c private_dtl. All function
 * pointers are set by the backend-specific init function and must not be
 * called before initialization or after finalization.
 *
 * The struct is aligned to 256 bytes to avoid false sharing across cache
 * lines in multi-threaded access patterns.
 *
 * The function pointer protocol for a consumer fetch is:
 * @code
 * rpc_pack()           // consumer: pack the fetch request
 * rpc_recv_response()  // consumer: record backend RPC state; does not block
 * establish_connection() // consumer: set up DTL data channel
 * recv()               // consumer: receive file data and obtain its buffer
 * close_connection()   // consumer: tear down DTL data channel
 * return_buffer()      // caller: release the received buffer after use
 * @endcode
 *
 * And for the producer service side:
 * @code
 * rpc_unpack()         // service: unpack the incoming fetch request
 * rpc_respond()        // service: backend RPC-control hook (currently no-op)
 * get_buffer()         // service: allocate send buffer
 * establish_connection() // service: set up DTL data channel
 * send()               // service: send file data
 * return_buffer()      // service: release send buffer
 * close_connection()   // service: tear down DTL data channel
 * @endcode
 */
struct dyad_dtl {
    dyad_dtl_private_t private_dtl;  ///< Opaque pointer to the active backend context.
    dyad_dtl_mode_t mode;            ///< Active DTL backend. @see dyad_dtl_mode_t.

    /**
     * @brief Packs a file fetch request into a JSON object for an RPC call.
     *
     * @param[in]  ctx           DYAD context.
     * @param[in]  upath         Relative path of the file to fetch.
     * @param[in]  producer_rank Flux rank of the producer broker.
     * @param[out] packed_obj    JSON object containing the packed request.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*rpc_pack) (const dyad_ctx_t *ctx,
                           const char *upath,
                           uint32_t producer_rank,
                           json_t **packed_obj);

    /**
     * @brief Unpacks a file fetch request from an incoming Flux RPC message.
     *
     * @param[in]  ctx        DYAD context.
     * @param[in]  packed_obj Incoming Flux RPC message.
     * @param[out] upath      Relative path of the requested file.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*rpc_unpack) (const dyad_ctx_t *ctx, const flux_msg_t *packed_obj, char **upath);

    /**
     * @brief Packs a byte-range fetch request into a JSON object for an RPC call.
     *
     * @details
     * Only implemented for @c DYAD_DTL_FLUX_RPC and @c DYAD_DTL_MARGO. Left
     * @c NULL for @c DYAD_DTL_UCX; callers must check @c ctx->dtl_handle->mode
     * before invoking this (see @c dyad_consume_range()).
     *
     * @param[in]  ctx           DYAD context.
     * @param[in]  upath         Relative path of the file to fetch from.
     * @param[in]  producer_rank Flux rank of the producer broker.
     * @param[in]  offset        Starting byte offset of the requested range.
     * @param[in]  length        Number of bytes requested.
     * @param[out] packed_obj    JSON object containing the packed request.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*rpc_pack_range) (const dyad_ctx_t *ctx,
                                 const char *upath,
                                 uint32_t producer_rank,
                                 size_t offset,
                                 size_t length,
                                 json_t **packed_obj);

    /**
     * @brief Unpacks a byte-range fetch request from an incoming Flux RPC message.
     *
     * @details
     * Only implemented for @c DYAD_DTL_FLUX_RPC and @c DYAD_DTL_MARGO. Left
     * @c NULL for @c DYAD_DTL_UCX.
     *
     * @param[in]  ctx        DYAD context.
     * @param[in]  packed_obj Incoming Flux RPC message.
     * @param[out] upath      Relative path of the requested file.
     * @param[out] offset     Starting byte offset of the requested range.
     * @param[out] length     Number of bytes requested.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*rpc_unpack_range) (const dyad_ctx_t *ctx,
                                   const flux_msg_t *packed_obj,
                                   char **upath,
                                   size_t *offset,
                                   size_t *length);

    /**
     * @brief Performs backend-specific RPC control work on the service.
     *
     * @details
     * The currently supported backends do not send a separate initial
     * acknowledgement, so their implementations are no-ops. The hook is
     * retained as part of the common DTL interface.
     *
     * @param[in] ctx      DYAD context.
     * @param[in] orig_msg The incoming Flux RPC message to respond to.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*rpc_respond) (const dyad_ctx_t *ctx, const flux_msg_t *orig_msg);

    /**
     * @brief Records backend state for the pending Flux RPC without waiting.
     *
     * @details
     * Flux RPC stores @p f for its later streaming receive. UCX and Margo
     * require no consumer-side RPC state here. This operation deliberately
     * does not wait for the producer, allowing the consumer to enter its
     * transport receive path without an extra handshake.
     *
     * @param[in] ctx DYAD context.
     * @param[in] f   Flux future representing the pending RPC response.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*rpc_recv_response) (const dyad_ctx_t *ctx, flux_future_t *f);

    /**
     * @brief Allocates a backend-managed buffer for data transfer.
     *
     * @details
     * For UCX this may be a pinned RDMA-registered buffer. For Flux RPC
     * it is a plain heap allocation. The buffer must be released via
     * @c return_buffer() when no longer needed.
     *
     * @param[in]  ctx       DYAD context.
     * @param[in]  data_size Required buffer size in bytes.
     * @param[out] data_buf  Pointer set to the allocated buffer.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*get_buffer) (const dyad_ctx_t *ctx, size_t data_size, void **data_buf);

    /**
     * @brief Releases a buffer previously allocated by @c get_buffer().
     *
     * @param[in]     ctx      DYAD context.
     * @param[in,out] data_buf Pointer to the buffer to release. Set to
     *                         @c NULL on return.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*return_buffer) (const dyad_ctx_t *ctx, void **data_buf);

    /**
     * @brief Establishes the DTL data channel between producer and consumer.
     *
     * @details
     * For UCX this creates a @c ucp_ep_h endpoint. For Flux RPC no
     * additional connection setup is required beyond the RPC itself.
     *
     * @param[in] ctx DYAD context.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*establish_connection) (const dyad_ctx_t *ctx);

    /**
     * @brief Sends file data to the consumer over the DTL data channel.
     *
     * @param[in] ctx    DYAD context.
     * @param[in] buf    Buffer containing the data to send.
     * @param[in] buflen Number of bytes to send.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*send) (const dyad_ctx_t *ctx, void *buf, size_t buflen);

    /**
     * @brief Receives file data from the producer over the DTL data channel.
     *
     * @param[in]  ctx    DYAD context.
     * @param[out] buf    Set to the buffer containing the received data.
     * @param[out] buflen Set to the number of bytes received.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*recv) (const dyad_ctx_t *ctx, void **buf, size_t *buflen);

    /**
     * @brief Tears down the DTL data channel.
     *
     * @param[in] ctx DYAD context.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*close_connection) (const dyad_ctx_t *ctx);

    /**
     * @brief Detaches the current request's DTL state from the shared,
     *        single-slot @c private_dtl fields into an independently-owned
     *        blob, so the request can be finished (@c rpc_send_detached())
     *        later/concurrently without racing a subsequent request's
     *        @c rpc_unpack_range() call, which would otherwise overwrite the
     *        same shared field (e.g. Flux RPC's @c msg, Margo's
     *        @c remote_addr).
     *
     * @details
     * Optional: @c NULL for backends that do not support detached/threaded
     * servicing (currently UCX). Callers must check for @c NULL before use
     * and fall back to the fully-synchronous @c establish_connection()/
     * @c send()/@c close_connection() sequence on the shared fields instead.
     *
     * Must be called on the same thread as -- and immediately after -- the
     * @c rpc_unpack_range() call whose state it is detaching, while the
     * shared field is still valid for this request.
     *
     * @param[in]  ctx       DYAD context.
     * @param[out] req_state Set to a newly allocated, backend-specific blob
     *                       owning this request's DTL state. Must be passed
     *                       to @c rpc_send_detached() exactly once to
     *                       complete the request and free it.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*rpc_detach_request) (const dyad_ctx_t *ctx, void **req_state);

    /**
     * @brief Sends file data to the consumer using a previously detached
     *        request's state, instead of the shared @c private_dtl fields.
     *
     * @details
     * Optional: @c NULL for backends that do not support detached/threaded
     * servicing (currently UCX). Equivalent to
     * @c establish_connection()+@c send()+@c close_connection(), but reads
     * per-request state from @p req_state instead of the shared fields, and
     * frees @p req_state before returning (success or failure).
     *
     * Whether this may be called from a thread other than the reactor
     * thread that received the request is backend-specific --
     * @see send_detached_is_thread_safe.
     *
     * @param[in] ctx       DYAD context.
     * @param[in] req_state Request state from a prior @c rpc_detach_request()
     *                      call. Freed by this call; must not be reused
     *                      afterward.
     * @param[in] buf       Buffer containing the data to send.
     * @param[in] buflen    Number of bytes to send.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*rpc_send_detached) (const dyad_ctx_t *ctx,
                                    void *req_state,
                                    void *buf,
                                    size_t buflen);

    /**
     * @brief Frees a detached request's state without sending data, for
     *        use on an I/O-error path where the fetch never reached
     *        @c rpc_send_detached().
     *
     * @details
     * Optional: @c NULL for backends that do not support detached/threaded
     * servicing (currently UCX; never called when @c rpc_detach_request is
     * @c NULL). Frees whatever @p req_state owns (e.g. Flux RPC's message
     * reference, Margo's resolved address) without attempting a send. Safe
     * to call from the same threads as @c rpc_send_detached() -- see
     * @c send_detached_is_thread_safe.
     *
     * @param[in] ctx       DYAD context.
     * @param[in] req_state Request state from a prior
     *                      @c rpc_detach_request() call. Freed by this
     *                      call; must not be reused afterward.
     * @return @c DYAD_RC_OK on success, or an error code on failure.
     */
    dyad_rc_t (*rpc_abort_detached) (const dyad_ctx_t *ctx, void *req_state);

    /**
     * @brief Whether @c rpc_send_detached() is safe to call from a worker
     *        thread other than the reactor thread that received the
     *        request.
     *
     * @details
     * @c false for both backends currently implemented. Flux RPC's
     * @c send() calls @c flux_respond_raw(), which touches the module's
     * @c flux_t handle and must only be touched from the reactor thread
     * that owns it. Margo's @c send() touches no Flux state but is
     * Argobots/Mercury state, and this DTL's producer-side @c margo_init()
     * creates no dedicated Argobots execution stream, so its @c mid is
     * only a valid execution context on the thread that called
     * @c margo_init() (the reactor thread) -- a plain worker-pool
     * @c pthread is not an Argobots ULT/execution-stream context, and
     * calling @c margo_forward() from one hangs. Callers must hand the
     * completed request back to the reactor thread (e.g. via a self-pipe +
     * reactor fd-watcher) before calling @c rpc_send_detached() for either
     * backend. This field exists for a future backend where it may
     * legitimately be @c true (e.g. one whose send touches neither Flux
     * nor an Argobots-only execution context). Meaningless (left @c false)
     * when @c rpc_send_detached is @c NULL.
     */
    bool send_detached_is_thread_safe;

} __attribute__ ((aligned (256)));
typedef struct dyad_dtl dyad_dtl_t;

/**
 * @brief Initializes the Data Transport Layer for a DYAD context.
 *
 * @details
 * Allocates the @c dyad_dtl handle inside @p ctx and delegates to the
 * backend-specific initialization function based on @p mode:
 *
 * - @c DYAD_DTL_UCX      → @c dyad_dtl_ucx_init()
 *   (only if built with @c DYAD_ENABLE_UCX_DTL)
 * - @c DYAD_DTL_MARGO    → @c dyad_dtl_margo_init()
 *   (only if built with @c DYAD_ENABLE_MARGO_DTL)
 * - @c DYAD_DTL_FLUX_RPC → @c dyad_dtl_flux_init()
 *   (always available)
 *
 * If @p mode does not match any enabled backend, returns
 * @c DYAD_RC_BADDTLMODE without initializing the handle.
 *
 * @param[in,out] ctx       DYAD context. On success, @c ctx->dtl_handle
 *                          is allocated and initialized.
 * @param[in]     mode      DTL backend to use. @see dyad_dtl_mode_t.
 * @param[in]     comm_mode Communication direction (@c DYAD_COMM_SEND
 *                          for producer, @c DYAD_COMM_RECV for consumer).
 *                          @see dyad_dtl_comm_mode_t.
 * @param[in]     debug     If @c true, enables verbose debug logging
 *                          in the DTL backend.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK        Initialization succeeded.
 * @retval DYAD_RC_SYSFAIL   Failed to allocate the @c dyad_dtl handle.
 * @retval DYAD_RC_BADDTLMODE @p mode does not match any enabled backend.
 * @retval other             Any error code returned by the backend-specific
 *                           init function.
 */
dyad_rc_t dyad_dtl_init (dyad_ctx_t *ctx,
                         dyad_dtl_mode_t mode,
                         dyad_dtl_comm_mode_t comm_mode,
                         bool debug);

/**
 * @brief Finalizes and frees the Data Transport Layer for a DYAD context.
 *
 * @details
 * Delegates to the backend-specific finalization function based on
 * @c ctx->dtl_handle->mode, then frees the @c dyad_dtl handle and sets
 * @c ctx->dtl_handle to @c NULL.
 *
 * If @c ctx->dtl_handle is already @c NULL, the function is a no-op and
 * returns @c DYAD_RC_OK. This allows @c dyad_dtl_finalize() to be called
 * safely on an already-finalized context without error.
 *
 * Backend dispatch:
 * - @c DYAD_DTL_UCX      → @c dyad_dtl_ucx_finalize()
 *   (only if built with @c DYAD_ENABLE_UCX_DTL and the UCX handle is
 *   non-@c NULL)
 * - @c DYAD_DTL_MARGO    → @c dyad_dtl_margo_finalize()
 *   (only if built with @c DYAD_ENABLE_MARGO_DTL)
 * - @c DYAD_DTL_FLUX_RPC → @c dyad_dtl_flux_finalize()
 *   (only if the Flux handle is non-@c NULL)
 *
 * @note The @c dyad_dtl handle is always freed and set to @c NULL
 *       regardless of whether the backend finalization succeeds or fails.
 *       Any error code returned by the backend is overwritten with
 *       @c DYAD_RC_OK before returning — this is intentional, as
 *       finalization errors are non-recoverable and should not prevent
 *       the caller from continuing teardown.
 *
 * @param[in,out] ctx  DYAD context. On return, @c ctx->dtl_handle is
 *                     @c NULL.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK         Finalization succeeded, or the handle was
 *                            already @c NULL.
 * @retval DYAD_RC_BADDTLMODE The mode stored in the handle does not match
 *                            any enabled backend. The handle is still freed.
 */
dyad_rc_t dyad_dtl_finalize (dyad_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* DYAD_DTL_DYAD_DTL_IMPL_H */
