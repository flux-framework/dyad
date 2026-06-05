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

#include <dyad/common/dyad_envs.h>
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/dtl/margo_dtl.h>
#include <dyad/utils/base64/base64.h>

#include <margo.h>
#include <mercury.h>
#include <mercury_macros.h>

// clang-format off
#define MARGO_MAX_TRANSFER_SIZE (@DYAD_DTL_MAX_TRANSFER_SIZE@)
// clang-format on

/**
 * @brief Mercury/Margo RPC input structure for a data transfer request.
 *
 * @details
 * The Mercury macro @c MERCURY_GEN_PROC generates the structure along with
 * the serialization functions. Contains the size of the data to
 * transfer and a bulk handle referencing the producer's registered memory
 * region for the RDMA pull.
 *
 * - @c n    — number of bytes to transfer.
 * - @c bulk — Mercury bulk handle to the producer's send buffer.
 */
MERCURY_GEN_PROC (margo_rpc_in_t, ((int64_t)(n)) ((hg_bulk_t)(bulk)))

/**
 * @brief Mercury/Margo RPC output structure for a data transfer response.
 *
 * @details
 * The Mercury macro @c MERCURY_GEN_PROC generates the structure along with
 * the serialization functions. Contains a return code sent back
 * to the producer after the RDMA pull completes.
 *
 * - @c ret — return code; 0 indicates success.
 */
MERCURY_GEN_PROC (margo_rpc_out_t, ((int32_t)(ret)))

/**
 * @brief Margo RPC handler that pulls file data from the producer via RDMA.
 *
 * @details
 * Registered as the handler for the DYAD Margo data transfer RPC.
 * Invoked on the consumer side when the producer calls
 * @c margo_forward() to initiate a data transfer. Performs the
 * following steps:
 *
 *  1. Retrieves the Margo instance and producer address from the
 *     RPC handle.
 *  2. Looks up the @c dyad_dtl_margo_t handle registered with the
 *     Margo instance via @c margo_registered_data().
 *  3. Unpacks the input (@c margo_rpc_in_t) to obtain the transfer
 *     size (@c n) and the producer's bulk handle.
 *  4. Allocates a receive buffer of @c n bytes and creates a local
 *     Mercury bulk handle with @c HG_BULK_WRITE_ONLY access.
 *  5. Performs an RDMA pull (@c HG_BULK_PULL) from the producer's
 *     bulk handle into the local buffer via @c margo_bulk_transfer().
 *  6. Responds to the producer with @c out.ret = 0 to signal
 *     completion, then frees the input and destroys the RPC handle.
 *  7. Sets @c margo_handle->recv_ready = 1 to unblock the consumer
 *     thread waiting in a busy loop on that flag.
 *
 * @note All Mercury/Margo calls are checked with @c assert(). This
 *       means any failure aborts the process rather than returning an
 *       error code. Error handling should be improved in a future
 *       revision.
 *
 * @note The receive buffer allocated in step 4 is stored in
 *       @c margo_handle->recv_buffer and must be freed by the caller
 *       after consuming the data.
 *
 * @note @c DEFINE_MARGO_RPC_HANDLER() wraps this function to register
 *       it with the Margo runtime as a ULT (user-level thread) handler.
 *
 * @todo Replace @c assert() calls with proper error handling that
 *       propagates failures back to the caller rather than aborting.
 *
 * @param[in] h  Mercury RPC handle for the incoming request.
 */
static void data_ready_rpc (hg_handle_t h)
{
    hg_return_t ret;
    margo_rpc_in_t in;
    margo_rpc_out_t out;
    // clang-format off
    (void) ret;
    // clang-format on

    margo_instance_id mid = margo_hg_handle_get_instance (h);

    const struct hg_info *info = margo_get_info (h);
    hg_addr_t producer_addr = info->addr;

    dyad_dtl_margo_t *margo_handle = (dyad_dtl_margo_t *)margo_registered_data (mid, info->id);

    ret = margo_get_input (h, &in);
    assert (ret == HG_SUCCESS);

    margo_handle->recv_len = (size_t)in.n;

    if (margo_handle->recv_len > margo_handle->bulk_buffer_size) {
        out.ret = -1;
        margo_respond (h, &out);
        margo_free_input (h, &in);
        margo_destroy (h);
        return;
    }

    // RDMA pull from the producer (which for now is the flux borker)
    ret = margo_bulk_transfer (mid,
                               HG_BULK_PULL,
                               producer_addr,
                               in.bulk,
                               0,
                               margo_handle->bulk_handle,
                               0,
                               margo_handle->recv_len);
    if (ret != HG_SUCCESS) {
        out.ret = -1;
        margo_respond (h, &out);
        margo_free_input (h, &in);
        margo_destroy (h);
        return;
    }
    margo_handle->recv_buffer = margo_handle->bulk_buffer;

    // DYAD_LOG_DEBUG(ctx, "[MARGO DTL] RDMA pulled from the producer.");

    out.ret = 0;
    ret = margo_respond (h, &out);
    assert (ret == HG_SUCCESS);
    ret = margo_free_input (h, &in);
    assert (ret == HG_SUCCESS);
    ret = margo_destroy (h);
    assert (ret == HG_SUCCESS);

    // set the ready flag so the client (should be in the busy-while
    // loop) can proceed with the pulled data.
    atomic_store_explicit (&margo_handle->recv_ready, true, memory_order_release);
}
DEFINE_MARGO_RPC_HANDLER (data_ready_rpc)

dyad_rc_t dyad_dtl_margo_get_buffer (const dyad_ctx_t *ctx, size_t data_size, void **data_buf)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;

    dyad_dtl_margo_t *margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    if (data_size > margo_handle->bulk_buffer_size) {
        DYAD_LOG_ERROR (ctx,
                        "[MARGO DTL] Requested data size (%zu) exceeds "
                        "pre-allocated bulk buffer size (%zu)",
                        data_size,
                        margo_handle->bulk_buffer_size);
        rc = DYAD_RC_BADBUF;
        goto margo_get_buf_done;
    }
    *data_buf = margo_handle->bulk_buffer;
    rc = DYAD_RC_OK;

margo_get_buf_done:
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_margo_return_buffer (const dyad_ctx_t *ctx, void **data_buf)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    if (data_buf == NULL) {
        rc = DYAD_RC_BADBUF;
        goto margo_ret_buf_done;
    }
    // Do NOT free — bulk_buffer is owned by the handle and freed in finalize
    *data_buf = NULL;
    rc = DYAD_RC_OK;

margo_ret_buf_done:
    DYAD_C_FUNCTION_END ();
    return rc;
}

/**
 * @brief Validates that a Mercury/libfabric network protocol is available
 *        on the current system.
 *
 * @details
 * Calls @c NA_Get_protocol_info() with @p protocol to check whether it
 * is supported by the installed libfabric providers.
 *
 * If the protocol is available, logs the class and device names at debug
 * level, frees the protocol info via @c NA_Free_protocol_info(), and
 * returns @c DYAD_RC_OK.
 *
 * If the protocol is not available, logs an error and attempts a second
 * call to @c NA_Get_protocol_info() with @c NULL to enumerate all
 * available protocols. Each available protocol is logged at debug level
 * to help the user identify a valid alternative. The protocol info is
 * freed and @c DYAD_RC_MARGO_BAD_PROTO is returned regardless of
 * whether the enumeration succeeds.
 *
 * If the enumeration call also fails or returns no protocols, logs a
 * message indicating that no NA protocol is available at all and returns
 * @c DYAD_RC_MARGO_BAD_PROTO.
 *
 * This function is called during Margo DTL initialization to fail fast
 * with a descriptive error message rather than letting @c margo_init()
 * fail with a cryptic error.
 *
 * @param[in] ctx      DYAD context.
 * @param[in] protocol Mercury/NA protocol string to validate, e.g.
 *                     @c "ofi+tcp" or @c "ofi+verbs". If @c NULL,
 *                     behavior is undefined — callers should always
 *                     pass a non-@c NULL string.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK               The protocol is available on this system.
 * @retval DYAD_RC_MARGO_BAD_PROTO  The protocol is not available, or
 *                                  @c NA_Get_protocol_info() failed.
 */
static dyad_rc_t validate_margo_protocol (const dyad_ctx_t *ctx, const char *protocol)
{
    struct na_protocol_info *info = NULL;
    na_return_t ret = NA_Get_protocol_info (protocol, &info);

    if (ret != NA_SUCCESS || info == NULL) {
        DYAD_LOG_ERROR (ctx,
                        "[MARGO DTL] NA protocol '%s' is not available "
                        "(NA_Get_protocol_info returned %d). ",
                        protocol,
                        (int)ret);
        if (info != NULL) {
            NA_Free_protocol_info (info);
            info = NULL;
        }
        ret = NA_Get_protocol_info (NULL, &info);
        if (ret != NA_SUCCESS || info == NULL) {
            DYAD_LOG_DEBUG (ctx, "[MARGO DTL] No NA protocol available\n");
            return DYAD_RC_MARGO_BAD_PROTO;
        }
        struct na_protocol_info *p = info;
        while (p != NULL) {
            DYAD_LOG_DEBUG (ctx,
                            "[MARGO DTL] class '%s' + protocol '%s' available\n",
                            p->class_name,
                            p->protocol_name);
            p = p->next;
        }
        NA_Free_protocol_info (info);
        return DYAD_RC_MARGO_BAD_PROTO;
    }

    DYAD_LOG_DEBUG (ctx,
                    "[MARGO DTL] NA protocol '%s' available "
                    "(class=%s, device=%s)",
                    protocol,
                    info->class_name,
                    info->device_name);
    NA_Free_protocol_info (info);
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_margo_init (const dyad_ctx_t *ctx,
                               dyad_dtl_mode_t mode,
                               dyad_dtl_comm_mode_t comm_mode,
                               bool debug)
{
    DYAD_C_FUNCTION_START ();

    // dyad_rc_t rc = DYAD_RC_OK;
    dyad_dtl_margo_t *margo_handle = NULL;
    dyad_rc_t rc = 0;
    hg_return_t hret = HG_SUCCESS;
    uint8_t bulk_access_mode = HG_BULK_READWRITE;

    ctx->dtl_handle->private_dtl.margo_dtl_handle = malloc (sizeof (struct dyad_dtl_margo));
    if (ctx->dtl_handle->private_dtl.margo_dtl_handle == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate internal Margo DTL context\n");
        DYAD_C_FUNCTION_END ();
        return DYAD_RC_SYSFAIL;
    }

    margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;
    margo_handle->h = (flux_t *)ctx->h;  // flux handle
    margo_handle->debug = debug;
    margo_handle->recv_ready = 0;
    margo_handle->bulk_buffer = NULL;
    margo_handle->bulk_handle = HG_BULK_NULL;
    margo_handle->bulk_buffer_size = MARGO_MAX_TRANSFER_SIZE;

    // Determine the Mercury network abstraction (NA) protocol (communication fabric) to use.
    //
    // Common values:
    //   ofi+tcp     – portable TCP/IP via libfabric (default)
    //   ofi+verbs   – InfiniBand via libfabric
    //   ofi+cxi     – HPE Slingshot (Cray EX) via libfabric
    //   sm          – shared memory (single-node testing)
    //   na+sm       – shared memory (newer Margo)
    //
    //   ucx+tcp:://    - TCP/IP;                  portable, works everywhere
    //   ucx+rc_v://    - InfiniBand RC (verbs);   low-latency IB
    //   ucx+rc_mlx5:// - InfiniBand RC (mlx5 optimized); Mellanox HCAs
    //   ucx+ud_v://    - InfiniBand UD (verbs);   scalable IB, less reliability overhead
    //   ucx+dc_mlx5:// - InfiniBand DC (mlx5);    large-scale IB (Frontier, Sierra)
    //   ucx+cma://     - Cross-Memory Attach;     intra-node shared memory (Linux)
    //   ucx+sysv://    - SysV shared memory;      intra-node only
    //
    // Example:
    //   export DYAD_MARGO_PROTO="ofi+verbs"

    const char *margo_na_protocol_env = getenv (DYAD_MARGO_PROTO_ENV);
    const char *margo_na_protocol =
        (margo_na_protocol_env != NULL && margo_na_protocol_env[0] != '\0') ? margo_na_protocol_env
                                                                            : "ofi+tcp";

    /* Validate before committing to margo_init() */
    dyad_rc_t rc = validate_margo_protocol (ctx, margo_na_protocol);
    if (rc != DYAD_RC_OK) {
        goto error;
    }

    if (comm_mode == DYAD_COMM_SEND) {
        // Producer (FLUX broker), essentially the Margo client
        margo_handle->mid = margo_init (margo_na_protocol, MARGO_CLIENT_MODE, 0, -1);
        if (margo_handle->mid == MARGO_INSTANCE_NULL) {
            DYAD_LOG_ERROR (ctx,
                            "[MARGO DTL] margo_init failed (SEND, protocol=%s)",
                            margo_na_protocol);
            goto error;
        }
        margo_handle->sendrecv_rpc_id = MARGO_REGISTER (margo_handle->mid,
                                                        "data_ready_rpc",
                                                        margo_rpc_in_t,
                                                        margo_rpc_out_t,
                                                        NULL);
        bulk_access_mode = HG_BULK_READ_ONLY;
    } else if (comm_mode == DYAD_COMM_RECV) {
        // Consumer (dyad client c wrapper), essentially the Margo server
        // The third argument indicates whether an Argobots execution stream (ES)
        // should be created to run the Mercury progress loop. If this argument is
        // set to 0, the progress loop is going to run in the context of the main
        // ES (this should be the standard scenario, unless you have a good reason
        // for not using the main ES, such as the main ES using MPI primitives that
        // could block the progress loop). A value of 1 will make Margo create an
        // ES to run the Mercury progress loop. The fourth argument is the number
        // of ES to create and use for executing RPC handlers. A value of 0 will
        // make Margo execute RPCs in the ES that called margo_init. A value of -1
        // will make Margo execute the RPCs in the ES running the progress loop.
        // A positive value will make Margo create new ESs to run the RPCs.
        margo_handle->mid = margo_init (margo_na_protocol, MARGO_SERVER_MODE, 1, -1);
        if (margo_handle->mid == MARGO_INSTANCE_NULL) {
            DYAD_LOG_ERROR (ctx,
                            "[MARGO DTL] margo_init failed (RECV, protocol=%s)",
                            margo_na_protocol);
            goto error;
        }
        margo_handle->sendrecv_rpc_id = MARGO_REGISTER (margo_handle->mid,
                                                        "data_ready_rpc",
                                                        margo_rpc_in_t,
                                                        margo_rpc_out_t,
                                                        data_ready_rpc);
        margo_register_data (margo_handle->mid, margo_handle->sendrecv_rpc_id, margo_handle, NULL);
        bulk_access_mode = HG_BULK_WRITE_ONLY;
    }
    margo_handle->bulk_buffer = malloc (margo_handle->bulk_buffer_size);
    if (margo_handle->bulk_buffer == NULL) {
        DYAD_LOG_ERROR (ctx, "[MARGO DTL] Failed to allocate persistent bulk buffer");
        goto error;
    }
    hret = margo_bulk_create (margo_handle->mid,
                              1,
                              &(margo_handle->bulk_buffer),
                              &(margo_handle->bulk_buffer_size),
                              bulk_access_mode,
                              &(margo_handle->bulk_handle));
    if (hret != HG_SUCCESS) {
        DYAD_LOG_ERROR (ctx, "[MARGO DTL] Failed to register persistent bulk buffer: %d", hret);
        goto error;
    }

    // both margo client and server
    margo_addr_self (margo_handle->mid, &margo_handle->local_addr);
    margo_handle->remote_addr = NULL;

    ctx->dtl_handle->rpc_pack = dyad_dtl_margo_rpc_pack;
    ctx->dtl_handle->rpc_unpack = dyad_dtl_margo_rpc_unpack;
    ctx->dtl_handle->rpc_respond = dyad_dtl_margo_rpc_respond;
    ctx->dtl_handle->rpc_recv_response = dyad_dtl_margo_rpc_recv_response;
    ctx->dtl_handle->get_buffer = dyad_dtl_margo_get_buffer;
    ctx->dtl_handle->return_buffer = dyad_dtl_margo_return_buffer;
    ctx->dtl_handle->establish_connection = dyad_dtl_margo_establish_connection;
    ctx->dtl_handle->send = dyad_dtl_margo_send;
    ctx->dtl_handle->recv = dyad_dtl_margo_recv;
    ctx->dtl_handle->close_connection = dyad_dtl_margo_close_connection;

    if (comm_mode == DYAD_COMM_SEND) {
        DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo dtl initialized - flux side");
    } else if (comm_mode == DYAD_COMM_RECV) {
        DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo dtl initialized - client side");
    }

    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;

    // clang-format off
error: __attribute__((unused));
    // clang-format on
    // If an error occured, finalize the DTL handle and
    // return a failing error code
    dyad_dtl_margo_finalize (ctx);
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_MARGOINIT_FAIL;
}

dyad_rc_t dyad_dtl_margo_rpc_pack (const dyad_ctx_t *ctx,
                                   const char *restrict upath,
                                   uint32_t producer_rank,
                                   json_t **restrict packed_obj)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    hg_return_t hg_rc;
    char *addr_str = NULL;

    dyad_dtl_margo_t *margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    // send my address (me as consumer and margo server)
    // We call margo_addr_to_string multiple times because there's no guarantee that addresses fit
    // in a statically sized buffer.
    // The first call to margo_addr_to_string is used to get the address size in bytes.
    // Then, the second call is used to actually get the valid address.
    //
    // Notably, this is necessary on Slingshot networks since Slingshot addresses are essentially
    // guaranteed to be greater than 128 bytes.
    size_t addr_str_size = 0;
    hg_rc =
        margo_addr_to_string (margo_handle->mid, NULL, &addr_str_size, margo_handle->local_addr);
    if (hg_rc != HG_SUCCESS) {
        DYAD_LOG_ERROR (ctx, "[MARGO DTL] margo_addr_to_string size query failed!");
        rc = DYAD_RC_SYSFAIL;
        goto dtl_margo_rpc_pack_region_finish;
    }
    addr_str = malloc (addr_str_size);
    if (addr_str == NULL) {
        rc = DYAD_RC_SYSFAIL;
        goto dtl_margo_rpc_pack_region_finish;
    }
    hg_rc = margo_addr_to_string (margo_handle->mid,
                                  addr_str,
                                  &addr_str_size,
                                  margo_handle->local_addr);
    if (hg_rc != HG_SUCCESS) {
        DYAD_LOG_ERROR (ctx, "[MARGO DTL] margo_addr_to_string failed!");
        rc = DYAD_RC_SYSFAIL;
        goto dtl_margo_rpc_pack_region_finish;
    }

    *packed_obj = json_pack ("{s:s, s:i, s:i, s:s%}",
                             "upath",  // s:s
                             upath,
                             "tag_prod",  // s:i
                             (int)producer_rank,
                             "pid_cons",  // s:s
                             ctx->pid,
                             "addr",  // s:s%
                             addr_str,
                             addr_str_size);

    if (*packed_obj == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not pack upath and Margo address for RPC.");
        rc = DYAD_RC_BADPACK;
        goto dtl_margo_rpc_pack_region_finish;
    }

    DYAD_LOG_DEBUG (ctx,
                    "[MARGO DTL] pack/send margo sever addr: %s, %ld.",
                    addr_str,
                    addr_str_size);

dtl_margo_rpc_pack_region_finish:;
    if (addr_str != NULL) {
        free (addr_str);
    }
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_margo_rpc_unpack (const dyad_ctx_t *ctx, const flux_msg_t *msg, char **upath)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;

    uint64_t tag_prod = 0;
    uint64_t pid = 0;
    char *addr_str = NULL;
    size_t addr_str_size = 0;
    int errcode;

    dyad_dtl_margo_t *margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    // retrive and decode the consumer margo-server address
    errcode = flux_request_unpack (msg,
                                   NULL,
                                   "{s:s, s:i, s:i, s:s%}",
                                   "upath",  // s:s
                                   upath,
                                   "tag_prod",  // s:i
                                   &tag_prod,
                                   "pid_cons",  // s:i
                                   &pid,
                                   "addr",  // s:s%
                                   &addr_str,
                                   &addr_str_size);
    if (errcode < 0) {
        DYAD_LOG_ERROR (ctx, "Could not unpack Flux message from consumer!\n");
        rc = DYAD_RC_BADUNPACK;
        goto dtl_margo_rpc_unpack_region_finish;
    }

    DYAD_LOG_DEBUG (ctx,
                    "[MARGO DTL] recv/unpack margo sever addr: %s, %ld.",
                    addr_str,
                    addr_str_size);
    margo_addr_lookup (margo_handle->mid, addr_str, &margo_handle->remote_addr);

dtl_margo_rpc_unpack_region_finish:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_margo_rpc_respond (const dyad_ctx_t *ctx, const flux_msg_t *orig_msg)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_margo_rpc_recv_response (const dyad_ctx_t *ctx, flux_future_t *f)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_margo_establish_connection (const dyad_ctx_t *ctx)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_margo_send (const dyad_ctx_t *ctx, void *buf, size_t buflen)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    hg_return_t ret = HG_SUCCESS;

    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo_send is called, buflen: %ld.", buflen);
    dyad_dtl_margo_t *margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    if (buflen > margo_handle->bulk_buffer_size) {
        DYAD_LOG_ERROR (ctx,
                        "[MARGO DTL] Send data (%zu bytes) exceeds bulk buffer size (%zu bytes)",
                        buflen,
                        margo_handle->bulk_buffer_size);
        DYAD_C_FUNCTION_END ();
        return DYAD_RC_BADBUF;
    }

    args.n = buflen;
    args.bulk = margo_handle->bulk_handle;

    // send a message to the consumer, notifying it that my data is ready
    hg_handle_t mh;
    ret = margo_create (margo_handle->mid,
                        margo_handle->remote_addr,
                        margo_handle->sendrecv_rpc_id,
                        &mh);
    if (ret != HG_SUCCESS) {
        DYAD_LOG_ERROR (ctx, "margo_create failed: %d", (int)ret);
        goto margo_error;
    }
    ret = margo_forward (mh, &args);
    if (ret != HG_SUCCESS) {
        DYAD_LOG_ERROR (ctx, "margo_forward failed: %d", (int)ret);
        goto margo_error;
    }

    margo_rpc_out_t resp;
    ret = margo_get_output (mh, &resp);
    if (ret != HG_SUCCESS) {
        DYAD_LOG_ERROR (ctx, "margo_get_output failed: %d", (int)ret);
        goto margo_error;
    }
    if (resp.ret != 0) {
        DYAD_LOG_ERROR (ctx, "[MARGO DTL] RPC handler returned error: %d", resp.ret);
        margo_free_output (h, &resp);
        margo_destroy (h);
        DYAD_C_FUNCTION_END ();
        goto margo_error;
    }
    margo_free_output (mh, &resp);
    margo_destroy (mh);

    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo_send completed, buflen: %lu", buflen);

    DYAD_C_FUNCTION_END ();
    return rc;

margo_error:;
    if (mh != HG_HANDLE_NULL) {
        margo_destroy (mh);
    }

    return DYAD_RC_MARGOINIT_FAIL;
}

dyad_rc_t dyad_dtl_margo_recv (const dyad_ctx_t *ctx, void **buf, size_t *buflen)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo_recv is called, waiting for data.");

    dyad_dtl_margo_t *margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    while (!atomic_load_explicit (&margo_handle->recv_ready, memory_order_acquire)) {
        usleep (100);
    }

    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo_recv received %ld bytes.", margo_handle->recv_len);

    // recv message handled, reset it to 0
    *buflen = margo_handle->recv_len;
    *buf = malloc (*buflen);
    if (*buf == NULL) {
        rc = DYAD_RC_SYSFAIL;
        goto recv_done;
    }
    memcpy (*buf, margo_handle->recv_buffer, margo_handle->recv_len);

    // margo_handle->recv_buffer is never actually allocated itself when in RECV mode.
    // Instead, data_ready_rpc() will set it to point to the margo_handle->bulk_buffer, which is
    // reused between different receive operations. So, we just need to set recv_buffer back to
    // NULL to "release" it.
    margo_handle->recv_buffer = NULL;
    margo_handle->recv_len = 0;
    atomic_store_explicit (&margo_handle->recv_ready, false, memory_order_relaxed);

recv_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_margo_close_connection (const dyad_ctx_t *ctx)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    // dyad_dtl_margo_t* margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;
    // dyad_dtl_comm_mode_t comm_mode = dtl_handle->comm_mode;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_margo_finalize (const dyad_ctx_t *ctx)
{
    DYAD_C_FUNCTION_START ();

    dyad_dtl_margo_t *margo_handle;
    dyad_rc_t rc = DYAD_RC_OK;

    if (ctx->dtl_handle == NULL || ctx->dtl_handle->private_dtl.margo_dtl_handle == NULL) {
        rc = DYAD_RC_OK;
        goto dtl_margo_finalize_finish;
    }

    margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    if (margo_handle->bulk_handle != HG_BULK_NULL) {
        margo_bulk_free (margo_handle->bulk_handle);
        margo_handle->bulk_handle = HG_BULK_NULL;
    }

    if (margo_handle->mid != MARGO_INSTANCE_NULL) {
        margo_addr_free (margo_handle->mid, margo_handle->local_addr);
        if (margo_handle->remote_addr != NULL)
            margo_addr_free (margo_handle->mid, margo_handle->remote_addr);
        margo_finalize (margo_handle->mid);
    }

    if (margo_handle->bulk_buffer != NULL) {
        free (margo_handle->bulk_buffer);
        margo_handle->bulk_buffer = NULL;
    }

    free (margo_handle);
    ctx->dtl_handle->private_dtl.margo_dtl_handle = NULL;

    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo dtl finalized");

dtl_margo_finalize_finish:;
    DYAD_C_FUNCTION_END ();
    return rc;
}
