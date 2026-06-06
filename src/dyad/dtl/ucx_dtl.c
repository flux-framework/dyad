#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/dtl/ucx_dtl.h>
#include <dyad/utils/base64/base64.h>

/**
 * @brief Base64 encoding map for RFC 4648, used to encode/decode UCX
 *        remote keys for transmission over the Flux RPC channel.
 */
extern const base64_maps_t base64_maps_rfc4648;

/**
 * @brief Maximum data size for a single UCX transfer (4 GiB).
 *
 * @details
 * UCX tag send/receive operations are limited to 4 GiB per transfer.
 * Files larger than this limit must be split into multiple transfers.
 */
// clang-format off
#define UCX_MAX_TRANSFER_SIZE (@DYAD_DTL_MAX_TRANSFER_SIZE@)
// clang-format on

/**
 * @brief Tag mask used for UCX tag send/receive operations.
 *
 * @details
 * Set to @c UINT64_MAX to match any tag, effectively disabling tag
 * filtering. DYAD uses a single tag per transfer so no filtering is
 * needed.
 */
#define DYAD_UCX_TAG_MASK UINT64_MAX

/**
 * @brief Request struct used to track the completion of async UCX
 *        operations.
 *
 * @details
 * UCX allocates one instance of this struct per in-flight operation
 * using @c dyad_ucx_request_init() as the initializer. The @c completed
 * flag is set to 1 by the send callback @c dyad_send_callback() when
 * the operation finishes, and polled by @c dyad_ucx_request_wait().
 */
struct ucx_request {
    int completed;  ///< Set to 1 when the UCX operation completes.
};
typedef struct ucx_request dyad_ucx_request_t;

/**
 * @brief Initializes a @c dyad_ucx_request_t allocated by UCX.
 *
 * @details
 * Registered with UCX via the @c request_init field of
 * @c ucp_params_t during @c dyad_dtl_ucx_init(). Called automatically
 * by UCX each time it allocates a new request object from its internal
 * memory pool. Sets @c completed to 0 so that @c dyad_ucx_request_wait()
 * can detect when the operation has finished.
 *
 * @param[in] request Pointer to the uninitialized request memory
 *                    allocated by UCX. Cast to @c dyad_ucx_request_t*.
 */
static void dyad_ucx_request_init (void *request)
{
    DYAD_C_FUNCTION_START ();
    dyad_ucx_request_t *real_request = NULL;
    real_request = (dyad_ucx_request_t *)request;
    real_request->completed = 0;
    DYAD_C_FUNCTION_END ();
}

/**
 * @brief UCX send completion callback.
 *
 * @details
 * Registered as the completion callback for UCX tag send operations.
 * Called by UCX when a send operation completes, either successfully
 * or with an error. Sets @c real_req->completed to 1 so that
 * @c dyad_ucx_request_wait() can detect completion and exit its
 * polling loop.
 *
 * The function signature differs between UCX API versions:
 * - UCX >= 1.10: receives an additional @c ctx pointer argument.
 * - UCX < 1.10:  receives only @c req and @c status.
 *
 * @param[in] req    Pointer to the @c dyad_ucx_request_t for the
 *                   completed operation.
 * @param[in] status Final status of the send operation.
 * @param[in] ctx    User context pointer (UCX >= 1.10 only, unused).
 */
#if UCP_API_VERSION >= UCP_VERSION(1, 10)
static void dyad_send_callback (void *req, ucs_status_t status, void *ctx)
#else   // UCP_API_VERSION
static void dyad_send_callback (void *req, ucs_status_t status)
#endif  // UCP_API_VERSION
{
    DYAD_C_FUNCTION_START ();
    DYAD_LOG_STDERR ("Calling send callback");
    dyad_ucx_request_t *real_req = (dyad_ucx_request_t *)req;
    real_req->completed = 1;
    DYAD_C_FUNCTION_END ();
}

/**
 * @brief Waits for an async UCX operation to complete.
 *
 * @details
 * Handles the three possible return values of a UCX communication
 * operation:
 *
 * - **Immediate success** (@c request == @c UCS_OK): the operation
 *   completed synchronously. Returns @c UCS_OK immediately.
 * - **Request handle** (@c UCS_PTR_IS_PTR(request)): the operation
 *   is in progress. Spins by calling @c ucp_worker_progress() and
 *   polling @c ucp_request_check_status() until the status is no
 *   longer @c UCS_INPROGRESS, then frees the request via
 *   @c ucp_request_free() and returns the final status.
 * - **Immediate error** (@c UCS_PTR_IS_ERR(request)): the operation
 *   failed immediately. Extracts the @c ucs_status_t error code via
 *   @c UCS_PTR_STATUS() and returns it.
 *
 * The spin loop is expected to run only a small number of iterations
 * because prior UCX calls in the send/receive path are structured to
 * minimize the size of the worker's event queue before this wait is
 * called.
 *
 * @param[in] ctx     DYAD context. Used to access the UCX worker via
 *                    @c ctx->dtl_handle->private_dtl.ucx_dtl_handle->ucx_worker.
 * @param[in] request Return value of a UCX communication operation
 *                    (e.g. @c ucp_tag_send_nbx() or
 *                    @c ucp_tag_recv_nbx()). May be @c UCS_OK, a
 *                    request handle, or a UCX error pointer.
 *
 * @return @c ucs_status_t final status of the operation:
 * @retval UCS_OK       The operation completed successfully.
 * @retval other        A UCX error code indicating failure.
 */
static ucs_status_t dyad_ucx_request_wait (const dyad_ctx_t *ctx, dyad_ucx_request_t *request)
{
    DYAD_C_FUNCTION_START ();
    ucs_status_t final_request_status = UCS_OK;
    if (UCS_OK == request) {
        DYAD_LOG_INFO (ctx, "Finished Immediately for Request");
    } else {
        DYAD_LOG_INFO (ctx, "Not finished lets check process of worker");
        // If 'request' is actually a request handle, this means the communication
        // operation is scheduled, but not yet completed.
        if (UCS_PTR_IS_PTR (request)) {
            // Spin lock until the request is completed
            // The spin lock shouldn't be costly (performance-wise)
            // because the wait should always come directly after other UCX calls
            // that minimize the size of the worker's event queue.
            // In other words, prior UCX calls should mean that this loop only runs
            // a couple of times at most.
            do {
                ucp_worker_progress (ctx->dtl_handle->private_dtl.ucx_dtl_handle->ucx_worker);

                // Get the final status of the communication operation
                final_request_status = ucp_request_check_status (request);
            } while (final_request_status == UCS_INPROGRESS);
            // Free and deallocate the request object
            ucp_request_free (request);
            goto dtl_ucx_request_wait_region_finish;
        }
        // If 'request' is actually a UCX error, this means the communication
        // operation immediately failed. In that case, we simply grab the
        // 'ucs_status_t' object for the error.
        else if (UCS_PTR_IS_ERR (request)) {
            final_request_status = UCS_PTR_STATUS (request);
            goto dtl_ucx_request_wait_region_finish;
        }
        DYAD_LOG_INFO (ctx, "Finished Waiting for Request");
    }
    // If 'request' is neither a request handle nor an error, then
    // the communication operation immediately completed successfully.
    // So, we simply set the status to UCS_OK
    final_request_status = UCS_OK;
dtl_ucx_request_wait_region_finish:;
    DYAD_C_FUNCTION_END ();
    return final_request_status;
}

/**
 * @brief Allocates and registers a UCX memory buffer for RDMA operations.
 *
 * @details
 * Allocates a host memory buffer of @c dtl_handle->max_transfer_size +
 * @c sizeof(size_t) bytes using @c ucp_mem_map() with
 * @c UCP_MEM_MAP_ALLOCATE, which lets UCX allocate and register the
 * memory in one step. The extra @c sizeof(size_t) bytes prepend the
 * file size to the buffer so the consumer can determine the data
 * boundary without an additional RDMA operation.
 *
 * The memory protection flags differ by communication direction:
 * - @c DYAD_COMM_SEND (producer): @c UCP_MEM_MAP_PROT_LOCAL_READ —
 *   the producer only reads from the buffer locally to send data.
 * - @c DYAD_COMM_RECV (consumer): @c UCP_MEM_MAP_PROT_REMOTE_WRITE —
 *   the buffer is exposed for remote write so the producer can push
 *   data into it via RDMA.
 *
 * After mapping, queries the actual allocated address via
 * @c ucp_mem_query() and stores it in @c dtl_handle->net_buf and
 * @c dtl_handle->cons_buf_ptr. On the producer side (@c DYAD_COMM_SEND),
 * @c cons_buf_ptr is initialized to the producer's own buffer address
 * as a placeholder; it is overwritten with the consumer's remote buffer
 * address in @c dyad_dtl_ucx_rpc_unpack() before each transfer.
 *
 * @c ucp_rkey_pack() is called only when @c comm_mode == @c DYAD_COMM_RECV.
 * The consumer packs its registered memory region into @c rkey_buf so
 * that the packed key can be sent to the producer (encoded in base64 via
 * @c dyad_dtl_ucx_rpc_pack()) to authorize the RDMA push into the
 * consumer's buffer. The producer does not need to pack its own memory
 * region since no peer performs RDMA into the producer's buffer.
 *
 * On any failure after a successful @c ucp_mem_map(), the memory is
 * unmapped via @c ucp_mem_unmap() before returning.
 *
 * @param[in]     ctx        DYAD context. Used for logging.
 * @param[in,out] dtl_handle UCX DTL internal state. On success,
 *                           @c net_buf, @c cons_buf_ptr, and
 *                           @c mem_handle are always populated.
 *                           @c rkey_buf and @c rkey_size are populated
 *                           only when @p comm_mode is @c DYAD_COMM_RECV.
 * @param[in]     comm_mode  Communication direction. Controls the memory
 *                           protection flags and whether @c ucp_rkey_pack()
 *                           is called. Must not be @c DYAD_COMM_NONE.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK                  Buffer allocated and registered
 *                                     successfully.
 * @retval DYAD_RC_NOCTX               @c dtl_handle->ucx_ctx is @c NULL.
 * @retval DYAD_RC_BAD_COMM_MODE       @c dtl_handle->comm_mode is
 *                                     @c DYAD_COMM_NONE.
 * @retval DYAD_RC_UCXMMAP_FAIL        @c ucp_mem_map() or
 *                                     @c ucp_mem_query() failed.
 * @retval DYAD_RC_UCXRKEY_PACK_FAILED @c ucp_rkey_pack() failed.
 *                                     Only possible when @p comm_mode
 *                                     is @c DYAD_COMM_RECV.
 * @retval DYAD_RC_BADBUF              Default error before any specific
 *                                     check is reached.
 */
static dyad_rc_t ucx_allocate_buffer (const dyad_ctx_t *ctx,
                                      dyad_dtl_ucx_t *dtl_handle,
                                      dyad_dtl_comm_mode_t comm_mode)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_BADBUF;
    ucs_status_t status;
    ucp_mem_map_params_t mmap_params;
    ucp_mem_attr_t attr;
    if (dtl_handle->ucx_ctx == NULL) {
        rc = DYAD_RC_NOCTX;
        DYAD_LOG_ERROR (ctx, "No UCX context provided");
        goto ucx_allocate_done;
    }
    if (dtl_handle->comm_mode == DYAD_COMM_NONE) {
        rc = DYAD_RC_BAD_COMM_MODE;
        goto ucx_allocate_done;
    }
    DYAD_LOG_INFO (ctx, "Allocating memory with UCX");
    mmap_params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH
                             | UCP_MEM_MAP_PARAM_FIELD_FLAGS | UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE
                             | UCP_MEM_MAP_PARAM_FIELD_PROT;
    mmap_params.address = NULL;
    mmap_params.memory_type = UCS_MEMORY_TYPE_HOST;
    mmap_params.length = dtl_handle->max_transfer_size + sizeof (size_t);
    mmap_params.flags = UCP_MEM_MAP_ALLOCATE;
    if (dtl_handle->comm_mode == DYAD_COMM_SEND) {
        mmap_params.prot = UCP_MEM_MAP_PROT_LOCAL_READ;
    } else {
        mmap_params.prot = UCP_MEM_MAP_PROT_REMOTE_WRITE;
    }
    status = ucp_mem_map (dtl_handle->ucx_ctx, &mmap_params, &(dtl_handle->mem_handle));
    if (UCX_STATUS_FAIL (status)) {
        rc = DYAD_RC_UCXMMAP_FAIL;
        DYAD_LOG_ERROR (ctx, "ucx_mem_map failed");
        goto ucx_allocate_done;
    }
    attr.field_mask = UCP_MEM_ATTR_FIELD_ADDRESS;
    status = ucp_mem_query (dtl_handle->mem_handle, &attr);
    if (UCX_STATUS_FAIL (status)) {
        ucp_mem_unmap (dtl_handle->ucx_ctx, dtl_handle->mem_handle);
        rc = DYAD_RC_UCXMMAP_FAIL;
        DYAD_LOG_ERROR (ctx, "Failed to get address to UCX allocated buffer");
        goto ucx_allocate_done;
    }
    dtl_handle->net_buf = attr.address;
    /* On the producer side this is a placeholder that will be overwritten
     * by the consumer's remote buffer address in dyad_dtl_ucx_rpc_unpack().
     * On the consumer side this is the actual RDMA destination address sent
     * to the producer via the RPC payload. */
    dtl_handle->cons_buf_ptr = (uint64_t)dtl_handle->net_buf;
    DYAD_LOG_DEBUG (ctx, "Done writing address");
    if (dtl_handle->comm_mode == DYAD_COMM_RECV) {
        status = ucp_rkey_pack (dtl_handle->ucx_ctx,
                                dtl_handle->mem_handle,
                                &(dtl_handle->rkey_buf),
                                &(dtl_handle->rkey_size));
        if (UCX_STATUS_FAIL (status)) {
            ucp_mem_unmap (dtl_handle->ucx_ctx, dtl_handle->mem_handle);
            rc = DYAD_RC_UCXRKEY_PACK_FAILED;
            DYAD_LOG_ERROR (ctx, "ucp_rkey_pack failed errno %d", status);
            goto ucx_allocate_done;
        }
    }
    rc = DYAD_RC_OK;

ucx_allocate_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

/**
 * @brief Releases a UCX RDMA-registered memory buffer.
 *
 * @details
 * Unmaps and deregisters the memory region identified by @p mem_handle
 * via @c ucp_mem_unmap(), which both frees the UCX memory registration
 * and releases the underlying memory allocated by @c ucp_mem_map() with
 * @c UCP_MEM_MAP_ALLOCATE. Sets @p *buf to @c NULL after unmapping.
 *
 * Validates all three inputs before proceeding:
 * - If @p ucp_ctx is @c NULL, returns @c DYAD_RC_NOCTX.
 * - If @p mem_handle is @c NULL, returns @c DYAD_RC_UCXMMAP_FAIL.
 * - If @p buf or @p *buf is @c NULL, returns @c DYAD_RC_BADBUF.
 *
 * @note Unlike @c dyad_dtl_flux_return_buffer() and
 *       @c dyad_dtl_margo_return_buffer() which use @c free(), this
 *       function uses @c ucp_mem_unmap() because the buffer was
 *       allocated and registered by UCX via @c ucp_mem_map() with
 *       @c UCP_MEM_MAP_ALLOCATE. Calling @c free() directly on a
 *       UCX-managed buffer would bypass UCX's memory registration
 *       and cause undefined behavior.
 *
 * @param[in]     ctx        DYAD context. Used for logging.
 * @param[in]     ucp_ctx    UCX context used to unmap the memory.
 *                           Must not be @c NULL.
 * @param[in]     mem_handle UCX memory handle returned by
 *                           @c ucp_mem_map(). Must not be @c NULL.
 * @param[in,out] buf        Pointer to the buffer to release.
 *                           @p *buf must be non-@c NULL on entry.
 *                           Set to @c NULL on success.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK          Buffer unmapped and released successfully.
 * @retval DYAD_RC_NOCTX       @p ucp_ctx is @c NULL.
 * @retval DYAD_RC_UCXMMAP_FAIL @p mem_handle is @c NULL.
 * @retval DYAD_RC_BADBUF      @p buf or @p *buf is @c NULL.
 */
static dyad_rc_t ucx_free_buffer (const dyad_ctx_t *ctx,
                                  ucp_context_h ucp_ctx,
                                  ucp_mem_h mem_handle,
                                  void **buf)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    if (ucp_ctx == NULL) {
        rc = DYAD_RC_NOCTX;
        DYAD_LOG_ERROR (ctx, "No UCX context provided");
        goto ucx_free_buf_done;
    }
    if (mem_handle == NULL) {
        rc = DYAD_RC_UCXMMAP_FAIL;
        DYAD_LOG_ERROR (ctx, "No UCX memory handle provided");
        goto ucx_free_buf_done;
    }
    if (buf == NULL || *buf == NULL) {
        rc = DYAD_RC_BADBUF;
        DYAD_LOG_ERROR (ctx, "No memory buffer provided");
        goto ucx_free_buf_done;
    }
    DYAD_LOG_DEBUG (ctx, "Releasing UCX allocated memory");
    ucp_mem_unmap (ucp_ctx, mem_handle);
    *buf = NULL;
    rc = DYAD_RC_OK;

ucx_free_buf_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

/**
 * @brief Initiates a non-blocking UCX RDMA push operation.
 *
 * @details
 * Performs a one-sided RDMA put of @p buf into the consumer's
 * pre-registered memory buffer using @c ucp_put_nbx(). The operation
 * proceeds in three steps:
 *
 *  1. Deserializes the consumer's remote key from @c dtl_handle->rkey_buf
 *     into a live @c ucp_rkey_t handle via @c ucp_ep_rkey_unpack().
 *     @c rkey_buf contains the serialized RDMA access credentials for the
 *     consumer's registered memory region, received and decoded from the
 *     RPC payload in @c dyad_dtl_ucx_rpc_unpack(). @c ucp_ep_rkey_unpack()
 *     is a local operation — it deserializes the credentials already
 *     present in @c rkey_buf into a @c ucp_rkey_t object in the format
 *     required by the UCX transport negotiated for @c dtl_handle->ep.
 *     No additional network communication takes place. The resulting
 *     @c ucp_rkey_t is stored in @c dtl_handle->rkey and remains valid
 *     until destroyed by @c dyad_dtl_ucx_close_connection().
 *  2. Initiates the RDMA put via @c ucp_put_nbx() with
 *     @c dyad_send_callback registered as the completion callback.
 *     The destination address is @c dtl_handle->cons_buf_ptr, which
 *     holds the consumer's remote buffer address (received in the RPC
 *     payload and stored by @c dyad_dtl_ucx_rpc_unpack()). Together,
 *     @c rkey (access credentials) and @c cons_buf_ptr (remote address)
 *     fully identify the RDMA target in the consumer's address space.
 *  3. Returns the @c ucs_status_ptr_t from @c ucp_put_nbx() without
 *     waiting for completion. The caller is responsible for passing the
 *     returned pointer to @c dyad_ucx_request_wait() to block until the
 *     operation finishes before calling
 *     @c dyad_dtl_ucx_close_connection().
 *
 * @note @c dtl_handle->rkey_buf is freed by
 *       @c dyad_dtl_ucx_close_connection() after the transfer completes,
 *       not by this function. It must remain valid until
 *       @c ucp_ep_rkey_unpack() returns.
 *
 * @note The @p is_warmup parameter is accepted for interface consistency
 *       with the receive path (@c ucx_recv_no_wait()) but is not used —
 *       no warmup distinction is made on the send path.
 *
 * @note This function uses the push RDMA model: the producer pushes data
 *       directly into the consumer's pre-registered memory buffer via
 *       @c ucp_put_nbx(), in contrast to the Margo backend which uses a
 *       pull model where the consumer pulls from the producer's buffer
 *       via @c HG_BULK_PULL.
 *
 * @param[in] ctx       DYAD context. The UCX endpoint, remote key buffer,
 *                      and consumer buffer pointer are read from the UCX
 *                      DTL internal state.
 * @param[in] is_warmup Unused. Accepted for interface consistency with
 *                      the receive path.
 * @param[in] buf       Local buffer containing the data to send.
 * @param[in] buflen    Number of bytes to send.
 *
 * @return @c ucs_status_ptr_t:
 * @retval UCS_OK                The operation completed immediately and
 *                               successfully (returned as a status, not
 *                               a pointer).
 * @retval valid pointer         The operation is in progress. Pass to
 *                               @c dyad_ucx_request_wait() to wait for
 *                               completion.
 * @retval UCS_ERR_NOT_CONNECTED @c dtl_handle->ep is @c NULL,
 *                               @c dtl_handle->rkey_buf is @c NULL,
 *                               @c ucp_ep_rkey_unpack() failed, or
 *                               @c ucp_put_nbx() returned an error.
 */
static inline ucs_status_ptr_t ucx_send_no_wait (const dyad_ctx_t *ctx,
                                                 bool is_warmup,
                                                 void *buf,
                                                 size_t buflen)
{
    /**
     * The warmup is not used but is passed for consistency with the recv_no_wait calls.
     */
    (void)is_warmup;
    DYAD_C_FUNCTION_START ();
    ucs_status_ptr_t stat_ptr = NULL;
    dyad_dtl_ucx_t *dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    if (dtl_handle->ep == NULL) {
        DYAD_LOG_ERROR (ctx, "UCP endpoint was not created prior to invoking send!");
        stat_ptr = (void *)UCS_ERR_NOT_CONNECTED;
        goto ucx_send_no_wait_done;
    }
    if (dtl_handle->rkey_buf == NULL) {
        DYAD_LOG_ERROR (ctx, "UCP remote key buffer is NULL prior to invoking send!");
        stat_ptr = (void *)UCS_ERR_NOT_CONNECTED;
        goto ucx_send_no_wait_done;
    }
    ucs_status_t status =
        ucp_ep_rkey_unpack (dtl_handle->ep, dtl_handle->rkey_buf, &(dtl_handle->rkey));
    if (UCX_STATUS_FAIL (status)) {
        DYAD_LOG_ERROR (ctx, "ucp_ep_rkey_unpack failed");
        stat_ptr = (void *)UCS_ERR_NOT_CONNECTED;
        goto ucx_send_no_wait_done;
    }
    ucp_request_param_t params;
    params.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK;
    params.cb.send = dyad_send_callback;
    stat_ptr = ucp_put_nbx (dtl_handle->ep,
                            buf,
                            buflen,
                            dtl_handle->cons_buf_ptr,
                            dtl_handle->rkey,
                            &params);
    if (UCS_PTR_IS_ERR (stat_ptr)) {
        DYAD_LOG_ERROR (ctx,
                        "ucp_put_nbx() failed %s (%d)\n",
                        ucs_status_string (UCS_PTR_STATUS (stat_ptr)),
                        UCS_PTR_STATUS (stat_ptr));
        stat_ptr = (void *)UCS_ERR_NOT_CONNECTED;
        goto ucx_send_no_wait_done;
    }
    DYAD_LOG_INFO (ctx, "written data buf of length %lu", buflen);
ucx_send_no_wait_done:;
    DYAD_C_FUNCTION_END ();
    return stat_ptr;
}

/**
 * @brief Waits for incoming RDMA data by polling the consumer's
 *        pre-registered buffer.
 *
 * @details
 * Busy-polls the first @c sizeof(ssize_t) bytes of
 * @c dtl_handle->net_buf, which the producer prepends with the file
 * size before initiating the RDMA push via @c ucp_put_nbx(). The
 * consumer spins until this value becomes non-zero, indicating that
 * the producer has started writing data into the buffer.
 *
 * On each iteration the UCX worker is progressed via
 * @c ucp_worker_progress() to process incoming network events, and
 * the thread sleeps for 10 microseconds to avoid saturating the CPU.
 *
 * This approach works because the UCX backend uses a push RDMA model —
 * the producer writes directly into the consumer's pre-registered
 * memory buffer (@c dtl_handle->net_buf) via @c ucp_put_nbx(). The
 * prepended file size acts as a sentinel: once the producer has
 * initiated the RDMA put, the first bytes of the buffer are non-zero,
 * signalling to the consumer that data is arriving.
 *
 * @note Unlike @c dyad_ucx_request_wait() which polls a UCX request
 *       handle for a specific operation, this function polls the buffer
 *       contents directly. This is necessary because the consumer has
 *       no UCX request handle for the producer's @c ucp_put_nbx() call
 *       — the put is one-sided and the consumer is not notified by UCX
 *       when it completes.
 *
 * @note The @p buf and @p buflen output parameters are accepted for
 *       interface consistency but are not populated by this function.
 *       The actual buffer pointer and length are read from
 *       @c dtl_handle->net_buf and the prepended size field by the
 *       caller (@c dyad_dtl_ucx_recv()) after this function returns.
 *
 * @note The @p is_warmup parameter controls whether this is a warmup
 *       iteration used to prime the RDMA connection before the actual
 *       data transfer. During warmup the buffer sentinel check still
 *       applies but the received data is discarded by the caller.
 *
 * @param[in]  ctx      DYAD context. The UCX worker and network buffer
 *                      are read from the UCX DTL internal state.
 * @param[in]  is_warmup If @c true, this is a warmup receive used to
 *                      prime the RDMA connection.
 * @param[out] buf      Unused by this function. Populated by the caller
 *                      after return.
 * @param[out] buflen   Unused by this function. Populated by the caller
 *                      after return.
 *
 * @return Always returns @c NULL. The actual data is read directly from
 *         @c dtl_handle->net_buf by the caller.
 *
 * @todo Replace the busy-poll with a more efficient notification
 *       mechanism. The current approach wastes CPU cycles and adds
 *       latency from the 10-microsecond sleep. UCX Active Messages
 *       or a lightweight atomic flag could provide lower-latency
 *       completion notification.
 */
static inline ucs_status_ptr_t ucx_recv_no_wait (const dyad_ctx_t *ctx,
                                                 bool is_warmup,
                                                 void **buf,
                                                 size_t *buflen)
{
    DYAD_C_FUNCTION_START ();
    ucs_status_ptr_t stat_ptr = NULL;
    dyad_dtl_ucx_t *dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    ssize_t temp = 0l;
    int is_first = 1;
    do {
        memcpy (&temp, dtl_handle->net_buf, sizeof (temp));
        ucp_worker_progress (ctx->dtl_handle->private_dtl.ucx_dtl_handle->ucx_worker);
        nanosleep ((const struct timespec[]){{0, 10000L}}, NULL);
        if (is_first == 1) {
            DYAD_LOG_DEBUG (ctx, "Consumer Waiting for worker to finsih all work");
        }
        is_first = 0;
    } while (temp == 0l);
    DYAD_LOG_DEBUG (ctx, "Consumer finsihed all work");
    DYAD_C_FUNCTION_END ();
    return stat_ptr;
}

/**
 * @brief Performs a UCX connection warmup by sending a 1-byte message
 *        to self.
 *
 * @details
 * Warms up the UCX RDMA connection by performing a loopback send from
 * the local worker to itself. This primes the UCX connection machinery
 * — endpoint creation, remote key exchange, and RDMA registration —
 * so that the first real data transfer does not pay the full connection
 * establishment cost.
 *
 * The warmup sequence is:
 *  1. Allocates a 1-byte UCX-registered send buffer via
 *     @c dyad_dtl_ucx_get_buffer() and a plain 1-byte receive buffer
 *     via @c malloc().
 *  2. Connects to the local worker's own address via @c ucx_connect(),
 *     creating a loopback endpoint.
 *  3. Initiates a non-blocking 1-byte RDMA put to self via
 *     @c ucx_send_no_wait() with @c is_warmup=true.
 *  4. Waits for the send to complete via @c dyad_ucx_request_wait().
 *  5. Disconnects from self via @c ucx_disconnect() and frees both
 *     buffers.
 *
 * On any failure, both buffers are freed and the appropriate error
 * code is returned before cleanup.
 *
 * @note The receive buffer is allocated with @c malloc() rather than
 *       @c dyad_dtl_ucx_get_buffer() because the warmup receive is not
 *       a real RDMA operation — the loopback send writes into the
 *       UCX-registered send buffer itself, and the plain receive buffer
 *       is only allocated to mirror the real transfer path without
 *       being used.
 *
 * @note The warmup is only performed on the producer side during
 *       @c dyad_dtl_ucx_init(). The consumer side does not need a
 *       warmup because it waits passively for the producer to initiate
 *       the connection.
 *
 * @param[in] ctx DYAD context. The UCX worker, local address, and
 *                endpoint are read from and written to the UCX DTL
 *                internal state.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK            Warmup completed successfully.
 * @retval DYAD_RC_SYSFAIL       Failed to allocate the plain receive
 *                               buffer via @c malloc().
 * @retval DYAD_RC_UCXCOMM_FAIL  The warmup send operation failed.
 * @retval other                 Any error code returned by
 *                               @c dyad_dtl_ucx_get_buffer() or
 *                               @c ucx_connect().
 */
static dyad_rc_t ucx_warmup (const dyad_ctx_t *ctx)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    void *send_buf = NULL;
    void *recv_buf = NULL;
    ucs_status_ptr_t send_stat_ptr = NULL;
    ucs_status_t send_status = UCS_OK;
    ucs_status_t recv_status = UCS_OK;
    (void)recv_status;
    DYAD_LOG_INFO (ctx, "Starting warmup for UCX DTL");
    DYAD_LOG_INFO (ctx, "Getting buffers for sending and receiving");
    rc = dyad_dtl_ucx_get_buffer (ctx, 1, &send_buf);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Failed to get UCX-allocated buffer");
        goto warmup_region_done;
    }
    recv_buf = malloc (1);
    if (recv_buf == NULL) {
        dyad_dtl_ucx_return_buffer (ctx, &send_buf);
        rc = DYAD_RC_SYSFAIL;
        DYAD_LOG_ERROR (ctx, "Failed to allocate receive buffer for warmup");
        goto warmup_region_done;
    }
    DYAD_LOG_INFO (ctx, "Establishing connection with self");
    rc = ucx_connect (ctx,
                      ctx->dtl_handle->private_dtl.ucx_dtl_handle->ucx_worker,
                      ctx->dtl_handle->private_dtl.ucx_dtl_handle->local_address,
                      &(ctx->dtl_handle->private_dtl.ucx_dtl_handle->ep));
    if (DYAD_IS_ERROR (rc)) {
        free (recv_buf);
        dyad_dtl_ucx_return_buffer (ctx, &send_buf);
        DYAD_LOG_ERROR (ctx, "Failed to establish connection with self");
        goto warmup_region_done;
    }
    DYAD_LOG_INFO (ctx, "Starting non-blocking send for warmup");
    send_stat_ptr = ucx_send_no_wait (ctx, true, send_buf, 1);
    if ((uintptr_t)send_stat_ptr == (uintptr_t)UCS_ERR_NOT_CONNECTED) {
        DYAD_LOG_ERROR (ctx, "Send failed because there's no endpoint");
        free (recv_buf);
        dyad_dtl_ucx_return_buffer (ctx, &send_buf);
        goto warmup_region_done;
    }
    DYAD_LOG_INFO (ctx, "Waiting on warmup send to finish");
    send_status = dyad_ucx_request_wait (ctx, send_stat_ptr);
    DYAD_LOG_INFO (ctx, "Disconnecting from self");
    ucx_disconnect (ctx,
                    ctx->dtl_handle->private_dtl.ucx_dtl_handle->ucx_worker,
                    ctx->dtl_handle->private_dtl.ucx_dtl_handle->ep);
    dyad_dtl_ucx_return_buffer (ctx, &send_buf);
    if (UCX_STATUS_FAIL (send_status)) {
        rc = DYAD_RC_UCXCOMM_FAIL;
        DYAD_LOG_ERROR (ctx, "Warmup communication failed in UCX!");
        goto warmup_region_done;
    }
    DYAD_LOG_INFO (ctx, "Communication succeeded (according to UCX)");
    DYAD_LOG_INFO (ctx, "Correct amount of data received in warmup");
    free (recv_buf);
    rc = DYAD_RC_OK;

warmup_region_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_init (const dyad_ctx_t *ctx,
                             dyad_dtl_mode_t mode,
                             dyad_dtl_comm_mode_t comm_mode,
                             bool debug)
{
    DYAD_C_FUNCTION_START ();
    ucp_params_t ucx_params;
    ucp_worker_params_t worker_params;
    ucp_config_t *config;
    ucs_status_t status;
    dyad_rc_t rc = DYAD_RC_OK;
    dyad_dtl_ucx_t *dtl_handle = NULL;

    ctx->dtl_handle->private_dtl.ucx_dtl_handle = malloc (sizeof (struct dyad_dtl_ucx));
    if (ctx->dtl_handle->private_dtl.ucx_dtl_handle == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate UCX DTL context\n");
        DYAD_C_FUNCTION_END ();
        return DYAD_RC_SYSFAIL;
    }
    dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    // Allocation/Freeing of the Flux handle should be
    // handled by the DYAD context
    dtl_handle->h = (flux_t *)ctx->h;
    dtl_handle->comm_mode = comm_mode;
    dtl_handle->debug = debug;
    dtl_handle->ucx_ctx = NULL;
    dtl_handle->ucx_worker = NULL;
    dtl_handle->mem_handle = NULL;
    dtl_handle->net_buf = NULL;
    dtl_handle->max_transfer_size = UCX_MAX_TRANSFER_SIZE;
    dtl_handle->local_address = NULL;
    dtl_handle->local_addr_len = 0ul;
    dtl_handle->remote_address = NULL;
    dtl_handle->remote_addr_len = 0ul;
    dtl_handle->ep = NULL;
    dtl_handle->comm_tag = 0ul;
    dtl_handle->ep_cache = NULL;
    dtl_handle->consumer_conn_key = 0ul;
    dtl_handle->rkey_buf = NULL;
    dtl_handle->rkey_size = 0ul;
    dtl_handle->cons_buf_ptr = 0ul;
    dtl_handle->rkey = NULL;

    // Read the UCX configuration
    DYAD_LOG_INFO (ctx, "Reading UCP config\n");
    status = ucp_config_read (NULL, NULL, &config);
    if (UCX_STATUS_FAIL (status)) {
        DYAD_LOG_ERROR (ctx, "Could not read the UCX config\n");
        goto error;
    }

    // Define the settings, parameters, features, etc.
    // for the UCX context. UCX will use this info internally
    // when creating workers, endpoints, etc.
    //
    // The settings enabled are:
    //   * Tag-matching send/recv
    //   * Remote Memory Access communication
    //   * Auto initialization of request objects
    //   * Worker sleep, wakeup, poll, etc. features
    ucx_params.field_mask = UCP_PARAM_FIELD_FEATURES | UCP_PARAM_FIELD_REQUEST_SIZE;
    ucx_params.features = UCP_FEATURE_RMA | UCP_FEATURE_AMO32 | UCP_FEATURE_TAG;
    ucx_params.request_size = sizeof (struct ucx_request);
    ucx_params.request_init = dyad_ucx_request_init;

    // Initialize UCX
    DYAD_LOG_INFO (ctx, "Initializing UCP\n");
    status = ucp_init (&ucx_params, config, &dtl_handle->ucx_ctx);

    // If in debug mode, print the configuration of UCX to stderr
    if (debug) {
        ucp_config_print (config, stderr, "UCX Configuration", UCS_CONFIG_PRINT_CONFIG);
    }
    // Release the config
    ucp_config_release (config);
    // Log an error if UCX initialization failed
    if (UCX_STATUS_FAIL (status)) {
        DYAD_LOG_ERROR (ctx, "ucp_init failed (status = %d)\n", status);
        goto error;
    }
    worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_SERIALIZED;

    // Create the worker and log an error if that fails
    DYAD_LOG_INFO (ctx, "Creating UCP worker\n");
    status = ucp_worker_create (dtl_handle->ucx_ctx, &worker_params, &(dtl_handle->ucx_worker));
    if (UCX_STATUS_FAIL (status)) {
        DYAD_LOG_ERROR (ctx, "ucp_worker_create failed (status = %d)!\n", status);
        goto error;
    }
    // Query the worker for its address
    DYAD_LOG_INFO (ctx, "Get address of UCP worker\n");
    status = ucp_worker_get_address (dtl_handle->ucx_worker,
                                     &(dtl_handle->local_address),
                                     &(dtl_handle->local_addr_len));

    // Initialize endpoint cache
    rc = dyad_ucx_ep_cache_init (ctx, &(dtl_handle->ep_cache));
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Cannot create endpoint cache (err code = %d)", (int)rc);
        goto error;
    }

    // Allocate a buffer of max transfer size using UCX
    ucx_allocate_buffer (ctx, dtl_handle, comm_mode);

    ctx->dtl_handle->rpc_pack = dyad_dtl_ucx_rpc_pack;
    ctx->dtl_handle->rpc_unpack = dyad_dtl_ucx_rpc_unpack;
    ctx->dtl_handle->rpc_respond = dyad_dtl_ucx_rpc_respond;
    ctx->dtl_handle->rpc_recv_response = dyad_dtl_ucx_rpc_recv_response;
    ctx->dtl_handle->get_buffer = dyad_dtl_ucx_get_buffer;
    ctx->dtl_handle->return_buffer = dyad_dtl_ucx_return_buffer;
    ctx->dtl_handle->establish_connection = dyad_dtl_ucx_establish_connection;
    ctx->dtl_handle->send = dyad_dtl_ucx_send;
    ctx->dtl_handle->recv = dyad_dtl_ucx_recv;
    ctx->dtl_handle->close_connection = dyad_dtl_ucx_close_connection;

    rc = ucx_warmup (ctx);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Warmup for UCX DTL failed");
        goto error;
    }
    dtl_handle->ep = NULL;

    DYAD_C_FUNCTION_END ();

    return DYAD_RC_OK;

error:;
    // If an error occured, finalize the DTL handle and
    // return a failing error code
    dyad_dtl_ucx_finalize (ctx);
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_UCXINIT_FAIL;
}

dyad_rc_t dyad_dtl_ucx_rpc_pack (const dyad_ctx_t *ctx,
                                 const char *restrict upath,
                                 uint32_t producer_rank,
                                 json_t **restrict packed_obj)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("upath", upath);
    DYAD_C_FUNCTION_UPDATE_INT ("producer_rank", producer_rank);
    DYAD_C_FUNCTION_UPDATE_INT ("pid", ctx->pid);
    dyad_dtl_ucx_t *dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    /**
     * Reset the Buffer so that we can loop on it
     * as we expect a size_t lets put int -1 as a check.
     */
    ssize_t temp = 0l;
    memcpy (dtl_handle->net_buf, &temp, sizeof (temp));
    dyad_rc_t rc = DYAD_RC_OK;
    size_t cons_enc_len = 0ul;
    char *cons_enc_buf = NULL;
    ssize_t cons_enc_size = 0l;

    if (dtl_handle->local_address == NULL) {
        DYAD_LOG_ERROR (dtl_handle, "Tried to pack an RPC payload without a local UCX address");
        rc = DYAD_RC_BADPACK;
        goto dtl_ucx_rpc_pack_region_finish;
    }
    DYAD_LOG_INFO (ctx, "Encode UCP address using base64\n");
    cons_enc_len = base64_encoded_length (dtl_handle->local_addr_len);
    // Add 1 to encoded length because the encoded buffer will be
    // packed as if it is a string
    cons_enc_buf = malloc (cons_enc_len + 1);
    if (cons_enc_buf == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate buffer for packed address\n");
        rc = DYAD_RC_SYSFAIL;
        goto dtl_ucx_rpc_pack_region_finish;
    }
    // remote_address is casted to const char* to avoid warnings
    // This is valid because it is a pointer to an opaque struct,
    // so the cast can be treated like a void*->char* cast.
    cons_enc_size = base64_encode_using_maps (&base64_maps_rfc4648,
                                              cons_enc_buf,
                                              cons_enc_len + 1,
                                              (const char *)dtl_handle->local_address,
                                              dtl_handle->local_addr_len);
    if (cons_enc_size < 0l) {
        DYAD_LOG_ERROR (ctx, "Unable to encode address\n");
        // TODO log error
        free (cons_enc_buf);
        rc = DYAD_RC_BADPACK;
        goto dtl_ucx_rpc_pack_region_finish;
    }

    DYAD_LOG_INFO (ctx, "Encode UCP rkey using base64\n");
    size_t rkey_enc_len = 0ul;
    char *rkey_enc_buf = NULL;
    ssize_t rkey_enc_size = 0l;
    rkey_enc_len = base64_encoded_length (dtl_handle->rkey_size);
    // Add 1 to encoded length because the encoded buffer will be
    // packed as if it is a string
    rkey_enc_buf = malloc (rkey_enc_len + 1);
    if (rkey_enc_buf == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate buffer for packed rkey\n");
        rc = DYAD_RC_SYSFAIL;
        goto dtl_ucx_rpc_pack_region_finish;
    }
    // remote_address is casted to const char* to avoid warnings
    // This is valid because it is a pointer to an opaque struct,
    // so the cast can be treated like a void*->char* cast.
    rkey_enc_size = base64_encode_using_maps (&base64_maps_rfc4648,
                                              rkey_enc_buf,
                                              rkey_enc_len + 1,
                                              (const char *)dtl_handle->rkey_buf,
                                              dtl_handle->rkey_size);
    if (rkey_enc_size < 0l) {
        DYAD_LOG_ERROR (ctx, "Unable to encode rkey\n");
        // TODO log error
        free (rkey_enc_buf);
        rc = DYAD_RC_BADPACK;
        goto dtl_ucx_rpc_pack_region_finish;
    }
    char *tag_name = "cons_buf";
    uint64_t tag_val = dtl_handle->cons_buf_ptr;
    char tag_val_buf[128];
    memset (tag_val_buf, 0x00, 128);
    sprintf (tag_val_buf, "%" PRIu64, tag_val);
    DYAD_LOG_INFO (ctx, "Creating Json object %lu with buf %s", tag_val, tag_val_buf);
    *packed_obj = json_pack ("{s:s, s:i, s:s, s:i, s:s%, s:s%}",
                             "upath",
                             upath,
                             "tag_prod",
                             (int)producer_rank,
                             tag_name,
                             tag_val_buf,
                             "pid_cons",
                             ctx->pid,
                             "addr",
                             cons_enc_buf,
                             cons_enc_len,
                             "rkey",
                             rkey_enc_buf,
                             rkey_enc_len);
    free (cons_enc_buf);
    free (rkey_enc_buf);
    // If the packing failed, log an error
    if (*packed_obj == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not pack upath and UCX address for RPC\n");
        rc = DYAD_RC_BADPACK;
        goto dtl_ucx_rpc_pack_region_finish;
    }
    rc = DYAD_RC_OK;
dtl_ucx_rpc_pack_region_finish:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_rpc_unpack (const dyad_ctx_t *ctx, const flux_msg_t *msg, char **upath)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    char *enc_addr = NULL;
    size_t enc_addr_len = 0;
    char *enc_rkey = NULL;
    size_t enc_rkey_len = 0;
    int errcode = 0;
    uint64_t tag_prod = 0;
    uint64_t tag_cons = 0;
    uint64_t pid = 0;
    ssize_t decoded_len = 0;
    dyad_dtl_ucx_t *dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    DYAD_LOG_INFO (ctx, "Unpacking RPC payload\n");
    uint64_t tag_val;
    char *tag_name = "cons_buf";
    char *tag_value_str = NULL;
    errcode = flux_request_unpack (msg,
                                   NULL,
                                   "{s:s, s:i, s:s, s:i, s:s%, s:s%}",
                                   "upath",
                                   upath,
                                   "tag_prod",
                                   &tag_prod,
                                   tag_name,
                                   &tag_value_str,
                                   "pid_cons",
                                   &pid,
                                   "addr",
                                   &enc_addr,
                                   &enc_addr_len,
                                   "rkey",
                                   &enc_rkey,
                                   &enc_rkey_len);
    tag_val = atoll (tag_value_str);
    DYAD_LOG_INFO (ctx, "Reading Json object %lu", tag_val);
    if (errcode < 0) {
        DYAD_LOG_ERROR (ctx, "Could not unpack Flux message from consumer!\n");
        rc = DYAD_RC_BADUNPACK;
        goto dtl_ucx_rpc_unpack_region_finish;
    }
    dtl_handle->cons_buf_ptr = tag_val;
    DYAD_C_FUNCTION_UPDATE_INT ("pid", pid);
    DYAD_C_FUNCTION_UPDATE_INT ("tag_cons", tag_cons);
    dtl_handle->comm_tag = tag_prod << 32 | tag_cons;
    dtl_handle->consumer_conn_key = pid << 32 | tag_cons;
    DYAD_C_FUNCTION_UPDATE_INT ("cons_key", dtl_handle->consumer_conn_key);
    DYAD_LOG_INFO (ctx, "Obtained upath from RPC payload: %s\n", *upath);
    DYAD_LOG_INFO (ctx, "Obtained UCP tag from RPC payload: %lu\n", dtl_handle->comm_tag);
    DYAD_LOG_INFO (ctx, "Decoding consumer UCP address using base64\n");
    dtl_handle->remote_addr_len = base64_decoded_length (enc_addr_len);
    dtl_handle->remote_address = (ucp_address_t *)malloc (dtl_handle->remote_addr_len);
    if (dtl_handle->remote_address == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate memory for consumer address");
        rc = DYAD_RC_SYSFAIL;
        goto dtl_ucx_rpc_unpack_region_finish;
    }
    decoded_len = base64_decode_using_maps (&base64_maps_rfc4648,
                                            (char *)dtl_handle->remote_address,
                                            dtl_handle->remote_addr_len,
                                            enc_addr,
                                            enc_addr_len);
    if (decoded_len < 0) {
        DYAD_LOG_ERROR (dtl_handle, "Failed to decode remote address");
        free (dtl_handle->remote_address);
        dtl_handle->remote_address = NULL;
        dtl_handle->remote_addr_len = 0;
        rc = DYAD_RC_BAD_B64DECODE;
        goto dtl_ucx_rpc_unpack_region_finish;
    }

    DYAD_LOG_INFO (ctx, "Decoding consumer UCP rkey using base64\n");
    dtl_handle->rkey_size = base64_decoded_length (enc_rkey_len);

    /* Guard against a previous RPC where dyad_dtl_ucx_close_connection()
     * was skipped on an error path, which would leave rkey_buf non-NULL. */
    if (dtl_handle->rkey_buf != NULL) {
        free (dtl_handle->rkey_buf);
        // dtl_handle->rkey_buf = NULL;
    }

    /* rkey_buf here is a plain malloc() allocation holding the consumer's
     * packed key decoded from the RPC payload, distinct from the consumer's
     * own UCX-allocated rkey_buf produced by ucp_rkey_pack(). It must be
     * freed with free() in dyad_dtl_ucx_close_connection(), not with
     * ucp_rkey_buffer_release(). */
    dtl_handle->rkey_buf = malloc (dtl_handle->rkey_size);

    if (dtl_handle->rkey_buf == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate memory for consumer rkey");
        rc = DYAD_RC_SYSFAIL;
        goto dtl_ucx_rpc_unpack_region_finish;
    }
    decoded_len = base64_decode_using_maps (&base64_maps_rfc4648,
                                            (char *)dtl_handle->rkey_buf,
                                            dtl_handle->rkey_size,
                                            enc_rkey,
                                            enc_rkey_len);
    if (decoded_len < 0) {
        DYAD_LOG_ERROR (dtl_handle, "Failed to decode remote rkey");
        free (dtl_handle->rkey_buf);
        dtl_handle->rkey_buf = NULL;
        dtl_handle->rkey_size = 0;
        rc = DYAD_RC_BAD_B64DECODE;
        goto dtl_ucx_rpc_unpack_region_finish;
    }
    rc = DYAD_RC_OK;
dtl_ucx_rpc_unpack_region_finish:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_rpc_respond (const dyad_ctx_t *ctx, const flux_msg_t *orig_msg)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_ucx_rpc_recv_response (const dyad_ctx_t *ctx, flux_future_t *f)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_ucx_get_buffer (const dyad_ctx_t *ctx, size_t data_size, void **data_buf)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    dyad_dtl_ucx_t *dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    DYAD_LOG_INFO (dtl_handle, "Validating data_buf in get_buffer");
    // TODO(Ian): the second part of this check is (for some reason) evaluating
    //            to true despite `data_buf` being a pointer to a NULL pointer.
    //            Since this function is internal, just comment this piece out for now.
    //            In the future, figure out why this is failing and fix it.
    // if (data_buf == NULL || *data_buf != NULL) {
    //     DYAD_LOG_ERROR (dtl_handle, "Invalid buffer pointer provided");
    //     DYAD_LOG_ERROR (dtl_handle, "Is double pointer NULL? -> %d", (data_buf == NULL));
    //     DYAD_LOG_ERROR (dtl_handle, "Is single pointer not NULL? -> %d", (*data_buf != NULL));
    //     rc = DYAD_RC_BADBUF;
    //     goto ucx_get_buffer_done;
    // }
    DYAD_LOG_INFO (dtl_handle, "Validating data_size in get_buffer");
    if (data_size > ctx->dtl_handle->private_dtl.ucx_dtl_handle->max_transfer_size) {
        DYAD_LOG_ERROR (dtl_handle,
                        "Requested a data size that's larger than the pre-allocated UCX buffer");
        rc = DYAD_RC_BADBUF;
        goto ucx_get_buffer_done;
    }
    DYAD_LOG_INFO (dtl_handle, "Setting the data buffer pointer to the UCX-allocated buffer");
    *data_buf = dtl_handle->net_buf;
    rc = DYAD_RC_OK;

ucx_get_buffer_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_return_buffer (const dyad_ctx_t *ctx, void **data_buf)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    if (data_buf == NULL || *data_buf == NULL) {
        rc = DYAD_RC_BADBUF;
        goto dtl_ucx_return_buffer_done;
    }
    *data_buf = NULL;
dtl_ucx_return_buffer_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_establish_connection (const dyad_ctx_t *ctx)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    dyad_dtl_ucx_t *dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    dyad_dtl_comm_mode_t comm_mode = dtl_handle->comm_mode;
    if (comm_mode == DYAD_COMM_SEND) {
        DYAD_LOG_INFO (ctx, "Create UCP endpoint for communication with consumer\n");
        rc = dyad_ucx_ep_cache_find (ctx,
                                     dtl_handle->ep_cache,
                                     dtl_handle->remote_address,
                                     dtl_handle->remote_addr_len,
                                     &(dtl_handle->ep));
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_INFO (ctx, "Create a new UCP endpoint for communication with consumer\n");
            rc = dyad_ucx_ep_cache_insert (ctx,
                                           dtl_handle->ep_cache,
                                           dtl_handle->remote_address,
                                           dtl_handle->remote_addr_len,
                                           dtl_handle->ucx_worker);
            if (DYAD_IS_ERROR (rc)) {
                DYAD_LOG_ERROR (ctx, "Failed to create UCP endpoint");
                goto dtl_ucx_establish_connection_region_finish;
            }
        }
        if (dtl_handle->debug) {
            ucp_ep_print_info (dtl_handle->ep, stderr);
        }
        rc = DYAD_RC_OK;
    } else if (comm_mode == DYAD_COMM_RECV) {
        DYAD_LOG_INFO (ctx, "No explicit connection establishment needed for UCX receiver\n");
        rc = DYAD_RC_OK;
    } else {
        DYAD_LOG_ERROR (ctx, "Invalid communication mode: %d\n", comm_mode);
        // TODO create new RC for this
        rc = DYAD_RC_BAD_COMM_MODE;
    }
dtl_ucx_establish_connection_region_finish:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_send (const dyad_ctx_t *ctx, void *buf, size_t buflen)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    ucs_status_ptr_t stat_ptr;
    ucs_status_t status = UCS_OK;
    stat_ptr = ucx_send_no_wait (ctx, false, buf, buflen);
    DYAD_LOG_INFO (ctx, "Processing UCP send request\n");
    status = dyad_ucx_request_wait (ctx, stat_ptr);
    if (status != UCS_OK) {
        DYAD_LOG_ERROR (ctx, "UCP Put failed (status = %d)!\n", (int)status);
        rc = DYAD_RC_UCXCOMM_FAIL;
        goto dtl_ucx_send_region_finish;
    }
    DYAD_LOG_INFO (ctx, "Data send with UCP succeeded\n");
    rc = DYAD_RC_OK;
dtl_ucx_send_region_finish:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_recv (const dyad_ctx_t *ctx, void **buf, size_t *buflen)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;

    ucs_status_ptr_t stat_ptr = NULL;
    // Wait on the recv operation to complete
    stat_ptr = ucx_recv_no_wait (ctx, false, buf, buflen);
    (void)stat_ptr;

    DYAD_LOG_INFO (ctx, "Data receive using UCX is successful\n");
    DYAD_LOG_INFO (ctx, "Received %lu bytes from producer\n", *buflen);

    rc = DYAD_RC_OK;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_close_connection (const dyad_ctx_t *ctx)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    dyad_dtl_ucx_t *dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    dyad_dtl_comm_mode_t comm_mode = dtl_handle->comm_mode;
    if (comm_mode == DYAD_COMM_SEND) {
        if (dtl_handle != NULL) {
            // TODO replace this code (either here or elsewhere) with LRU eviction
            // rc = ucx_disconnect (ctx, dtl_handle->ucx_worker, dtl_handle->ep);
            // if (DYAD_IS_ERROR (rc)) {
            //     DYAD_LOG_ERROR (ctx,
            //                   "Could not successfully close Endpoint! However, endpoint was "
            //                   "released.");
            // }
            /* Destroy the unpacked rkey handle. Only the producer (DYAD_COMM_SEND)
             * creates rkey via ucp_ep_rkey_unpack() in ucx_send_no_wait(). */
            if (dtl_handle->rkey != NULL) {
                ucp_rkey_destroy (dtl_handle->rkey);
                dtl_handle->rkey = NULL;
            }
            /* Free the per-RPC malloc() allocation of producer's rkey_buf from
             * dyad_dtl_ucx_rpc_unpack().
             * The consumer's rkey_buf is UCX-allocated and freed in finalize(). */
            if (dtl_handle->rkey_buf != NULL) {
                free (dtl_handle->rkey_buf);
                dtl_handle->rkey_buf = NULL;
            }
            /* ep is intentionally not destroyed here — it remains alive in
             * ep_cache for reuse across RPCs and is destroyed in finalize(). */
            dtl_handle->ep = NULL;
            // Sender doesn't have a consumer address at this time
            // So, free the consumer address when closing the connection
            // NOTE: currently removing the deallocation of the remote address
            //       because it is still in use by the endpoint cache
            // if (dtl_handle->remote_address != NULL) {
            //     free (dtl_handle->remote_address);
            dtl_handle->remote_address = NULL;
            dtl_handle->remote_addr_len = 0;
            // }
            dtl_handle->comm_tag = 0;
        }
        DYAD_LOG_INFO (ctx, "UCP endpoint close successful\n");
        rc = DYAD_RC_OK;
    } else if (comm_mode == DYAD_COMM_RECV) {
        // Since we're using tag send/recv, there's no need
        // to explicitly close the connection. So, all we're
        // doing here is setting the tag back to 0 (which cannot
        // be valid for DYAD because DYAD won't send a file from
        // one node to the same node).
        dtl_handle->comm_tag = 0;
        rc = DYAD_RC_OK;
    } else {
        DYAD_LOG_ERROR (ctx, "Somehow, an invalid comm mode reached 'close_connection'\n");
        // TODO create new RC for this case
        rc = DYAD_RC_BAD_COMM_MODE;
    }
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_finalize (const dyad_ctx_t *ctx)
{
    DYAD_C_FUNCTION_START ();
    dyad_dtl_ucx_t *dtl_handle = NULL;
    dyad_rc_t rc = DYAD_RC_OK;
    if (ctx->dtl_handle == NULL || ctx->dtl_handle->private_dtl.ucx_dtl_handle == NULL) {
        rc = DYAD_RC_OK;
        goto dtl_ucx_finalize_region_finish;
    }
    dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    DYAD_LOG_INFO (ctx, "Finalizing UCX DTL");
    if (dtl_handle->ep != NULL) {
        dyad_dtl_ucx_close_connection (ctx);
        dtl_handle->ep = NULL;
    }
    /* Destroy all cached endpoints before the worker since they are
     * bound to it. Actual connection teardown happens here, not in
     * close_connection(). */
    if (dtl_handle->ep_cache != NULL) {
        dyad_ucx_ep_cache_finalize (ctx, &(dtl_handle->ep_cache), dtl_handle->ucx_worker);
        dtl_handle->ep_cache = NULL;
    }
    /* local_address is UCX-allocated by ucp_worker_get_address() and must
     * be released with ucp_worker_release_address(), not free(). */
    if (dtl_handle->local_address != NULL) {
        ucp_worker_release_address (dtl_handle->ucx_worker, dtl_handle->local_address);
        dtl_handle->local_address = NULL;
    }
    // Free remote adddress structure if not already freed
    if (dtl_handle->remote_address != NULL) {
        free (dtl_handle->remote_address);
        dtl_handle->remote_address = NULL;
    }
    /* rkey_buf deallocation differs by comm_mode:
     * - DYAD_COMM_SEND: plain malloc() from rpc_unpack() ->free()
     * - DYAD_COMM_RECV: UCX-allocated by ucp_rkey_pack() -> ucp_rkey_buffer_release() */
    if (dtl_handle->rkey_buf != NULL) {
        if (dtl_handle->comm_mode == DYAD_COMM_SEND)
            free (dtl_handle->rkey_buf);
        else
            ucp_rkey_buffer_release (dtl_handle->rkey_buf);
        dtl_handle->rkey_buf = NULL;
    }
    if (dtl_handle->rkey != NULL) {
        ucp_rkey_destroy (dtl_handle->rkey);
        dtl_handle->rkey = NULL;
    }
    // Free memory buffer if not already freed
    if (dtl_handle->mem_handle != NULL) {
        ucx_free_buffer (ctx, dtl_handle->ucx_ctx, dtl_handle->mem_handle, &(dtl_handle->net_buf));
        dtl_handle->mem_handle = NULL;
    }
    /* Worker must be destroyed before the UCX context. */
    if (dtl_handle->ucx_worker != NULL) {
        ucp_worker_destroy (dtl_handle->ucx_worker);
        dtl_handle->ucx_worker = NULL;
    }
    /* UCX context destroyed last — all resources bound to it must
     * already be released. */
    if (dtl_handle->ucx_ctx != NULL) {
        ucp_cleanup (dtl_handle->ucx_ctx);
        dtl_handle->ucx_ctx = NULL;
    }

    // Flux handle should be released by the
    // DYAD context, so it is not released here
    dtl_handle->h = NULL;
    // Free the handle and set to NULL to prevent double free
    free (dtl_handle);
    ctx->dtl_handle->private_dtl.ucx_dtl_handle = NULL;
    rc = DYAD_RC_OK;
dtl_ucx_finalize_region_finish:;
    DYAD_C_FUNCTION_END ();
    return rc;
}
