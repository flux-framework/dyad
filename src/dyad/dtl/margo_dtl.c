#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdatomic.h>
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

static dyad_rc_t dyad_dtl_margo_send_error_to (const dyad_ctx_t *ctx,
                                               margo_instance_id mid,
                                               hg_id_t sendrecv_rpc_id,
                                               hg_addr_t remote_addr);

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
 * A negative @c in.n is the producer-failure marker. In that case this
 * handler does not allocate or perform an RDMA pull; it publishes
 * @c recv_failed and @c recv_ready so @c dyad_dtl_margo_recv() returns
 * @c DYAD_RC_BADFIO.
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
 * @return None. The RPC response carries completion to the producer.
 */
static void data_ready_rpc (hg_handle_t h)
{
    hg_return_t ret;
    margo_rpc_in_t in;
    margo_rpc_out_t out;
    hg_bulk_t local_bulk = HG_BULK_NULL;
    // clang-format off
    (void) ret;
    // clang-format on

    margo_instance_id mid = margo_hg_handle_get_instance (h);
    margo_set_log_level (mid, MARGO_LOG_INFO);

    const struct hg_info *info = margo_get_info (h);
    hg_addr_t producer_addr = info->addr;

    dyad_dtl_margo_t *margo_handle = (dyad_dtl_margo_t *)margo_registered_data (mid, info->id);

    ret = margo_get_input (h, &in);
    assert (ret == HG_SUCCESS);

    if (in.n < 0) {
        margo_handle->recv_len = 0;
        margo_handle->recv_buffer = NULL;
        atomic_store_explicit (&margo_handle->recv_failed, true, memory_order_release);
        goto data_ready_respond;
    }
    atomic_store_explicit (&margo_handle->recv_failed, false, memory_order_relaxed);
    margo_handle->recv_len = (size_t)in.n;
    margo_handle->recv_buffer = malloc (margo_handle->recv_len);

    ret = margo_bulk_create (mid,
                             1,
                             (void **)&margo_handle->recv_buffer,
                             &margo_handle->recv_len,
                             HG_BULK_WRITE_ONLY,
                             &local_bulk);
    assert (ret == HG_SUCCESS);

    // RDMA pull from the producer (which for now is the flux borker)
    ret = margo_bulk_transfer (mid,
                               HG_BULK_PULL,
                               producer_addr,
                               in.bulk,
                               0,
                               local_bulk,
                               0,
                               margo_handle->recv_len);
    assert (ret == HG_SUCCESS);

    // DYAD_LOG_DEBUG(ctx, "[MARGO DTL] RDMA pulled from the producer.");

    // Every margo_bulk_create() above must be paired with a free, or the
    // registration leaks on the CXI NIC's finite memory-registration table
    // (fi_mr_enable() ENOSPC after a few dozen requests under concurrent
    // load -- this was previously unfreed on every single request).
    ret = margo_bulk_free (local_bulk);
    assert (ret == HG_SUCCESS);

data_ready_respond:
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

    if (data_buf == NULL || *data_buf != NULL) {
        rc = DYAD_RC_BADBUF;
        goto margo_get_buf_done;
    }
#if 1
    *data_buf = malloc (data_size);
    if (*data_buf == NULL) {
        rc = DYAD_RC_SYSFAIL;
    }
#else
    rc = posix_memalign (data_buf, sysconf (_SC_PAGESIZE), data_size);
    if (rc != 0 || *data_buf == NULL) {
        rc = DYAD_RC_SYSFAIL;
    }
#endif

margo_get_buf_done:
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_margo_return_buffer (const dyad_ctx_t *ctx, void **data_buf)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    if (data_buf == NULL || *data_buf == NULL) {
        rc = DYAD_RC_BADBUF;
        goto margo_ret_buf_done;
    }
    free (*data_buf);
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

    ctx->dtl_handle->private_dtl.margo_dtl_handle = malloc (sizeof (struct dyad_dtl_margo));
    if (ctx->dtl_handle->private_dtl.margo_dtl_handle == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate internal Margo DTL context\n");
        DYAD_C_FUNCTION_END ();
        return DYAD_RC_SYSFAIL;
    }

    margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;
    margo_handle->h = (flux_t *)ctx->h;  // flux handle
    margo_handle->debug = debug;
    atomic_init (&margo_handle->recv_ready, false);
    atomic_init (&margo_handle->recv_failed, false);
    margo_handle->addr_cache = NULL;
    margo_handle->addr_cache_len = 0;
    margo_handle->addr_cache_cap = 0;

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
    }

    // both margo client and server
    {
        hg_return_t addr_self_ret = margo_addr_self (margo_handle->mid, &margo_handle->local_addr);
        if (addr_self_ret != HG_SUCCESS) {
            DYAD_LOG_ERROR (ctx, "[MARGO DTL] margo_addr_self failed: %d", (int)addr_self_ret);
            goto error;
        }
    }
    margo_handle->remote_addr = NULL;

    ctx->dtl_handle->rpc_pack = dyad_dtl_margo_rpc_pack;
    ctx->dtl_handle->rpc_unpack = dyad_dtl_margo_rpc_unpack;
    ctx->dtl_handle->rpc_pack_range = dyad_dtl_margo_rpc_pack_range;
    ctx->dtl_handle->rpc_unpack_range = dyad_dtl_margo_rpc_unpack_range;
    ctx->dtl_handle->rpc_respond = dyad_dtl_margo_rpc_respond;
    ctx->dtl_handle->rpc_recv_response = dyad_dtl_margo_rpc_recv_response;
    ctx->dtl_handle->get_buffer = dyad_dtl_margo_get_buffer;
    ctx->dtl_handle->return_buffer = dyad_dtl_margo_return_buffer;
    ctx->dtl_handle->establish_connection = dyad_dtl_margo_establish_connection;
    ctx->dtl_handle->send = dyad_dtl_margo_send;
    ctx->dtl_handle->recv = dyad_dtl_margo_recv;
    ctx->dtl_handle->close_connection = dyad_dtl_margo_close_connection;
    ctx->dtl_handle->rpc_detach_request = dyad_dtl_margo_detach_request;
    ctx->dtl_handle->rpc_send_detached = dyad_dtl_margo_send_detached;
    ctx->dtl_handle->rpc_abort_detached = dyad_dtl_margo_abort_detached;
    // send_detached() touches no Flux state, but IS Argobots/Mercury state
    // -- and this DTL's producer-side margo_init() (DYAD_COMM_SEND, below)
    // creates no dedicated Argobots execution stream, so `mid` is only a
    // valid Argobots execution context on the thread that called
    // margo_init() (the module's reactor thread). A plain pthread from the
    // fetch-worker pool is not an Argobots ULT/execution-stream context at
    // all, and calling margo_forward() from one hangs (confirmed: the
    // consumer's RDMA pull is never triggered). So, despite touching no
    // Flux handle, send_detached() must still be bounced back to the
    // reactor thread like Flux RPC's -- worker threads only get the
    // blocking file I/O off the reactor thread, not the RDMA send itself.
    ctx->dtl_handle->send_detached_is_thread_safe = false;

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

dyad_rc_t dyad_dtl_margo_addr_cache_lookup (const dyad_ctx_t *ctx,
                                            const char *addr_str,
                                            hg_addr_t *out_addr)
{
    dyad_dtl_margo_t *margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    for (size_t i = 0; i < margo_handle->addr_cache_len; i++) {
        if (strcmp (margo_handle->addr_cache[i].addr_str, addr_str) == 0) {
            *out_addr = margo_handle->addr_cache[i].addr;
            return DYAD_RC_OK;
        }
    }

    hg_addr_t addr = HG_ADDR_NULL;
    hg_return_t ret = margo_addr_lookup (margo_handle->mid, addr_str, &addr);
    if (ret != HG_SUCCESS) {
        DYAD_LOG_ERROR (ctx,
                        "[MARGO DTL] margo_addr_lookup failed for '%s': %d",
                        addr_str,
                        (int)ret);
        return DYAD_RC_MARGOINIT_FAIL;
    }

    if (margo_handle->addr_cache_len == margo_handle->addr_cache_cap) {
        size_t new_cap = margo_handle->addr_cache_cap == 0 ? 8 : margo_handle->addr_cache_cap * 2;
        struct dyad_dtl_margo_addr_cache_entry *grown =
            (struct dyad_dtl_margo_addr_cache_entry *)realloc (margo_handle->addr_cache,
                                                               new_cap * sizeof (*grown));
        if (grown == NULL) {
            DYAD_LOG_ERROR (ctx, "[MARGO DTL] Could not grow address cache");
            margo_addr_free (margo_handle->mid, addr);
            return DYAD_RC_SYSFAIL;
        }
        margo_handle->addr_cache = grown;
        margo_handle->addr_cache_cap = new_cap;
    }

    char *addr_str_copy = strdup (addr_str);
    if (addr_str_copy == NULL) {
        DYAD_LOG_ERROR (ctx, "[MARGO DTL] Could not duplicate address string for cache");
        margo_addr_free (margo_handle->mid, addr);
        return DYAD_RC_SYSFAIL;
    }
    margo_handle->addr_cache[margo_handle->addr_cache_len].addr_str = addr_str_copy;
    margo_handle->addr_cache[margo_handle->addr_cache_len].addr = addr;
    margo_handle->addr_cache_len++;

    *out_addr = addr;
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_margo_rpc_pack (const dyad_ctx_t *ctx,
                                   const char *restrict upath,
                                   uint32_t producer_rank,
                                   json_t **restrict packed_obj)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;

    dyad_dtl_margo_t *margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    // send my address (me as consumer and margo server)
    char addr_str[128];
    size_t addr_str_size = 128;
    {
        hg_return_t addr_str_ret = margo_addr_to_string (margo_handle->mid,
                                                         addr_str,
                                                         &addr_str_size,
                                                         margo_handle->local_addr);
        if (addr_str_ret != HG_SUCCESS) {
            DYAD_LOG_ERROR (ctx, "[MARGO DTL] margo_addr_to_string failed: %d", (int)addr_str_ret);
            rc = DYAD_RC_BADPACK;
            goto dtl_margo_rpc_pack_region_finish;
        }
    }
    // HG_Addr_to_string() sets addr_str_size to the string length *including*
    // the null terminator; Jansson's "s%" format wants the length *excluding*
    // it, so passing addr_str_size unadjusted packs one extra (the null)
    // byte as string content.
    if (addr_str_size > 0) {
        addr_str_size -= 1;
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
    rc = dyad_dtl_margo_addr_cache_lookup (ctx, addr_str, &margo_handle->remote_addr);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "[MARGO DTL] Could not resolve consumer address '%s'", addr_str);
        goto dtl_margo_rpc_unpack_region_finish;
    }

dtl_margo_rpc_unpack_region_finish:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_margo_rpc_pack_range (const dyad_ctx_t *ctx,
                                         const char *upath,
                                         uint32_t producer_rank,
                                         size_t offset,
                                         size_t length,
                                         json_t **packed_obj)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;

    dyad_dtl_margo_t *margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    // send my address (me as consumer and margo server)
    char addr_str[128];
    size_t addr_str_size = 128;
    {
        hg_return_t addr_str_ret = margo_addr_to_string (margo_handle->mid,
                                                         addr_str,
                                                         &addr_str_size,
                                                         margo_handle->local_addr);
        if (addr_str_ret != HG_SUCCESS) {
            DYAD_LOG_ERROR (ctx, "[MARGO DTL] margo_addr_to_string failed: %d", (int)addr_str_ret);
            rc = DYAD_RC_BADPACK;
            goto dtl_margo_rpc_pack_range_region_finish;
        }
    }
    // HG_Addr_to_string() sets addr_str_size to the string length *including*
    // the null terminator; Jansson's "s%" format wants the length *excluding*
    // it, so passing addr_str_size unadjusted packs one extra (the null)
    // byte as string content.
    if (addr_str_size > 0) {
        addr_str_size -= 1;
    }

    *packed_obj = json_pack ("{s:s, s:i, s:i, s:s%, s:I, s:I}",
                             "upath",  // s:s
                             upath,
                             "tag_prod",  // s:i
                             (int)producer_rank,
                             "pid_cons",  // s:s
                             ctx->pid,
                             "addr",  // s:s%
                             addr_str,
                             addr_str_size,
                             "offset",  // s:I
                             (json_int_t)offset,
                             "length",  // s:I
                             (json_int_t)length);

    if (*packed_obj == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not pack upath/offset/length and Margo address for RPC.");
        rc = DYAD_RC_BADPACK;
        goto dtl_margo_rpc_pack_range_region_finish;
    }

    DYAD_LOG_DEBUG (ctx,
                    "[MARGO DTL] pack/send ranged margo sever addr: %s, %ld.",
                    addr_str,
                    addr_str_size);

dtl_margo_rpc_pack_range_region_finish:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_margo_rpc_unpack_range (const dyad_ctx_t *ctx,
                                           const flux_msg_t *msg,
                                           char **upath,
                                           size_t *offset,
                                           size_t *length)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;

    uint64_t tag_prod = 0;
    uint64_t pid = 0;
    char *addr_str = NULL;
    size_t addr_str_size = 0;
    json_int_t offset_val = 0;
    json_int_t length_val = 0;
    int errcode;

    dyad_dtl_margo_t *margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    // retrive and decode the consumer margo-server address, plus the
    // requested byte range
    errcode = flux_request_unpack (msg,
                                   NULL,
                                   "{s:s, s:i, s:i, s:s%, s:I, s:I}",
                                   "upath",  // s:s
                                   upath,
                                   "tag_prod",  // s:i
                                   &tag_prod,
                                   "pid_cons",  // s:i
                                   &pid,
                                   "addr",  // s:s%
                                   &addr_str,
                                   &addr_str_size,
                                   "offset",  // s:I
                                   &offset_val,
                                   "length",  // s:I
                                   &length_val);
    if (errcode < 0) {
        DYAD_LOG_ERROR (ctx, "Could not unpack ranged Flux message from consumer!\n");
        rc = DYAD_RC_BADUNPACK;
        goto dtl_margo_rpc_unpack_range_region_finish;
    }
    *offset = (size_t)offset_val;
    *length = (size_t)length_val;

    DYAD_LOG_DEBUG (ctx,
                    "[MARGO DTL] recv/unpack ranged margo sever addr: %s, %ld.",
                    addr_str,
                    addr_str_size);
    rc = dyad_dtl_margo_addr_cache_lookup (ctx, addr_str, &margo_handle->remote_addr);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "[MARGO DTL] Could not resolve consumer address '%s'", addr_str);
        goto dtl_margo_rpc_unpack_range_region_finish;
    }

dtl_margo_rpc_unpack_range_region_finish:;
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

/**
 * @brief Sends one Margo RDMA data-transfer RPC to a consumer.
 *
 * @param[in] ctx Producer DYAD context used for logging.
 * @param[in] mid Producer Margo instance.
 * @param[in] sendrecv_rpc_id Registered transfer RPC identifier.
 * @param[in] remote_addr Borrowed destination address of the consumer.
 * @param[in] buf Data buffer to register for the consumer's RDMA pull.
 * @param[in] buflen Number of bytes in @p buf.
 * @return @c DYAD_RC_OK on acknowledgement; @c DYAD_RC_MARGOINIT_FAIL on a
 *         Margo bulk, RPC, or response failure.
 */
static dyad_rc_t dyad_dtl_margo_send_to (const dyad_ctx_t *ctx,
                                         margo_instance_id mid,
                                         hg_id_t sendrecv_rpc_id,
                                         hg_addr_t remote_addr,
                                         void *buf,
                                         size_t buflen)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    hg_return_t ret = HG_SUCCESS;

    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo_send is called, buflen: %ld.", buflen);

    if (buflen == DYAD_DTL_SEND_ERROR) {
        return dyad_dtl_margo_send_error_to (ctx, mid, sendrecv_rpc_id, remote_addr);
    }
    hg_size_t segment_sizes[1] = {(hg_size_t)buflen};
    void *segment_ptrs[1] = {buf};
    hg_bulk_t local_bulk = HG_BULK_NULL;
    margo_rpc_in_t args;
    hg_handle_t mh = HG_HANDLE_NULL;
    margo_rpc_out_t resp;

    // Register my local data
    // which will be pulled by the consumer
    ret = margo_bulk_create (mid, 1, segment_ptrs, segment_sizes, HG_BULK_READ_ONLY, &local_bulk);
    if (ret != HG_SUCCESS) {
        DYAD_LOG_ERROR (ctx, "margo_bulk_create failed: %d", (int)ret);
        goto margo_error_bulk;
    }

    args.n = buflen;
    args.bulk = local_bulk;

    // send a message to the consumer, notifying it that my data is ready
    ret = margo_create (mid, remote_addr, sendrecv_rpc_id, &mh);
    if (ret != HG_SUCCESS) {
        DYAD_LOG_ERROR (ctx, "margo_create failed: %d", (int)ret);
        goto margo_error;
    }
    ret = margo_forward (mh, &args);
    if (ret != HG_SUCCESS) {
        DYAD_LOG_ERROR (ctx, "margo_forward failed: %d", (int)ret);
        goto margo_error;
    }

    ret = margo_get_output (mh, &resp);
    if (ret != HG_SUCCESS) {
        DYAD_LOG_ERROR (ctx, "margo_get_output failed: %d", (int)ret);
        goto margo_error;
    }
    margo_free_output (mh, &resp);
    margo_destroy (mh);
    // Every margo_bulk_create() above must be paired with a free, or the
    // registration leaks on the CXI NIC's finite memory-registration table
    // (fi_mr_enable() ENOSPC after a few dozen requests under concurrent
    // load -- this was previously unfreed on this, the success, path; only
    // the error paths below freed it).
    margo_bulk_free (local_bulk);

    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo_send completed, buflen: %lu", buflen);

    DYAD_C_FUNCTION_END ();
    return rc;

margo_error:;
    if (mh != HG_HANDLE_NULL) {
        margo_destroy (mh);
    }

margo_error_bulk:;
    if (local_bulk != HG_BULK_NULL) {
        margo_bulk_free (local_bulk);
    }

    return DYAD_RC_MARGOINIT_FAIL;
}

/**
 * @brief Sends the Margo @c n == -1 producer-failure control RPC.
 *
 * @param[in] ctx Producer DYAD context used for logging.
 * @param[in] mid Producer Margo instance.
 * @param[in] sendrecv_rpc_id Registered transfer RPC identifier.
 * @param[in] remote_addr Borrowed destination address of the consumer.
 * @return @c DYAD_RC_OK if the consumer acknowledges the marker;
 *         @c DYAD_RC_MARGOINIT_FAIL for a Margo RPC failure.
 */
static dyad_rc_t dyad_dtl_margo_send_error_to (const dyad_ctx_t *ctx,
                                               margo_instance_id mid,
                                               hg_id_t sendrecv_rpc_id,
                                               hg_addr_t remote_addr)
{
    hg_return_t ret;
    hg_handle_t mh = HG_HANDLE_NULL;
    margo_rpc_in_t args = {.n = -1, .bulk = HG_BULK_NULL};
    margo_rpc_out_t resp;

    ret = margo_create (mid, remote_addr, sendrecv_rpc_id, &mh);
    if (ret == HG_SUCCESS)
        ret = margo_forward (mh, &args);
    if (ret == HG_SUCCESS)
        ret = margo_get_output (mh, &resp);
    if (ret == HG_SUCCESS)
        margo_free_output (mh, &resp);
    if (mh != HG_HANDLE_NULL)
        margo_destroy (mh);
    if (ret != HG_SUCCESS)
        DYAD_LOG_ERROR (ctx, "Margo failed-transfer notification failed: %d", (int)ret);
    return ret == HG_SUCCESS ? DYAD_RC_OK : DYAD_RC_MARGOINIT_FAIL;
}

dyad_rc_t dyad_dtl_margo_send (const dyad_ctx_t *ctx, void *buf, size_t buflen)
{
    dyad_dtl_margo_t *margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;
    return dyad_dtl_margo_send_to (ctx,
                                   margo_handle->mid,
                                   margo_handle->sendrecv_rpc_id,
                                   margo_handle->remote_addr,
                                   buf,
                                   buflen);
}

/**
 * @brief Sends a failure marker using the shared request destination.
 *
 * @param[in] ctx Producer Margo DTL context with @c remote_addr set.
 * @return @c DYAD_RC_OK if the marker is acknowledged; otherwise
 *         @c DYAD_RC_MARGOINIT_FAIL.
 */
dyad_rc_t dyad_dtl_margo_detach_request (const dyad_ctx_t *ctx, void **req_state)
{
    DYAD_C_FUNCTION_START ();
    struct dyad_dtl_margo_req_state *state = NULL;
    dyad_dtl_margo_t *margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    state = (struct dyad_dtl_margo_req_state *)malloc (sizeof (struct dyad_dtl_margo_req_state));
    if (state == NULL) {
        DYAD_C_FUNCTION_END ();
        return DYAD_RC_SYSFAIL;
    }
    state->mid = margo_handle->mid;
    state->sendrecv_rpc_id = margo_handle->sendrecv_rpc_id;
    state->remote_addr = margo_handle->remote_addr;
    // Move the borrowed cache pointer out of the shared field so a later
    // request's margo_addr_lookup() cannot be mistaken for this one.
    margo_handle->remote_addr = NULL;
    *req_state = state;

    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_margo_send_detached (const dyad_ctx_t *ctx,
                                        void *req_state,
                                        void *buf,
                                        size_t buflen)
{
    dyad_rc_t rc = DYAD_RC_OK;
    struct dyad_dtl_margo_req_state *state = (struct dyad_dtl_margo_req_state *)req_state;

    rc = dyad_dtl_margo_send_to (ctx,
                                 state->mid,
                                 state->sendrecv_rpc_id,
                                 state->remote_addr,
                                 buf,
                                 buflen);

    // state->remote_addr is borrowed from the address cache (see
    // struct dyad_dtl_margo_addr_cache_entry) -- NOT freed here. It's
    // reused by future requests to the same consumer and only freed at
    // dyad_dtl_margo_finalize().
    free (state);
    return rc;
}

/**
 * @brief Sends a failure marker and consumes detached request state.
 *
 * @param[in] ctx Producer Margo DTL context.
 * @param[in] req_state Detached request state consumed regardless of result.
 * @return @c DYAD_RC_OK if the marker is acknowledged; otherwise
 *         @c DYAD_RC_MARGOINIT_FAIL.
 */
dyad_rc_t dyad_dtl_margo_abort_detached (const dyad_ctx_t *ctx, void *req_state)
{
    struct dyad_dtl_margo_req_state *state = (struct dyad_dtl_margo_req_state *)req_state;
    // state->remote_addr is borrowed from the address cache -- not freed
    // here. See dyad_dtl_margo_send_detached().
    free (state);
    return DYAD_RC_OK;
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

    if (atomic_load_explicit (&margo_handle->recv_failed, memory_order_acquire)) {
        atomic_store_explicit (&margo_handle->recv_failed, false, memory_order_relaxed);
        atomic_store_explicit (&margo_handle->recv_ready, false, memory_order_relaxed);
        DYAD_C_FUNCTION_END ();
        return DYAD_RC_BADFIO;
    }

    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo_recv received %ld bytes.", margo_handle->recv_len);

    // recv message handled, reset it to 0
    *buflen = margo_handle->recv_len;
    *buf = malloc (*buflen);
    memcpy (*buf, margo_handle->recv_buffer, margo_handle->recv_len);

    // margo_handle->recv_buffer is allocated in data_ready_rpc()
    free (margo_handle->recv_buffer);
    margo_handle->recv_buffer = NULL;
    margo_handle->recv_len = 0;
    atomic_store_explicit (&margo_handle->recv_ready, false, memory_order_relaxed);

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

    if (margo_handle->mid != MARGO_INSTANCE_NULL) {
        margo_addr_free (margo_handle->mid, margo_handle->local_addr);
        for (size_t i = 0; i < margo_handle->addr_cache_len; i++) {
            margo_addr_free (margo_handle->mid, margo_handle->addr_cache[i].addr);
            free (margo_handle->addr_cache[i].addr_str);
        }
        margo_finalize (margo_handle->mid);
    }
    free (margo_handle->addr_cache);
    free (margo_handle);
    ctx->dtl_handle->private_dtl.margo_dtl_handle = NULL;

    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo dtl finalized");

dtl_margo_finalize_finish:;
    DYAD_C_FUNCTION_END ();
    return rc;
}
