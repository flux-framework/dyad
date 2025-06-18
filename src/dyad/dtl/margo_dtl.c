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
#include <dyad/dtl/margo_dtl.h>
#include <dyad/utils/base64/base64.h>

#include <margo.h>
#include <mercury.h>
#include <mercury_macros.h>

/* We use the Mercury macros to define the input
 * and output structures along with the serialization
 * functions.
 */
MERCURY_GEN_PROC (margo_rpc_in_t, ((int64_t)(n)) ((hg_bulk_t)(bulk)))
MERCURY_GEN_PROC (margo_rpc_out_t, ((int32_t)(ret)))

static void data_ready_rpc (hg_handle_t h)
{
    hg_return_t ret;
    margo_rpc_in_t in;
    margo_rpc_out_t out;
    hg_bulk_t local_bulk;
    // clang-format off
    (void) ret;
    // clang-format on

    margo_instance_id mid = margo_hg_handle_get_instance (h);
    margo_set_log_level (mid, MARGO_LOG_INFO);

    const struct hg_info* info = margo_get_info (h);
    hg_addr_t producer_addr = info->addr;

    dyad_dtl_margo_t* margo_handle = (dyad_dtl_margo_t*)margo_registered_data (mid, info->id);

    ret = margo_get_input (h, &in);
    assert (ret == HG_SUCCESS);

    margo_handle->recv_len = (size_t)in.n;
    margo_handle->recv_buffer = malloc (margo_handle->recv_len);

    ret = margo_bulk_create (mid,
                             1,
                             (void**)&margo_handle->recv_buffer,
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

    out.ret = 0;
    ret = margo_respond (h, &out);
    assert (ret == HG_SUCCESS);
    ret = margo_free_input (h, &in);
    assert (ret == HG_SUCCESS);
    ret = margo_destroy (h);
    assert (ret == HG_SUCCESS);

    // set the ready flag so the client (should be in the busy-while
    // loop) can proceed with the pulled data.
    margo_handle->recv_ready = 1;
}
DEFINE_MARGO_RPC_HANDLER (data_ready_rpc)

dyad_rc_t dyad_dtl_margo_get_buffer (const dyad_ctx_t* ctx, size_t data_size, void** data_buf)
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

dyad_rc_t dyad_dtl_margo_return_buffer (const dyad_ctx_t* ctx, void** data_buf)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    if (data_buf == NULL || *data_buf == NULL) {
        rc = DYAD_RC_BADBUF;
        goto margo_ret_buf_done;
    }
    free (*data_buf);
    rc = DYAD_RC_OK;

margo_ret_buf_done:
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_margo_init (const dyad_ctx_t* ctx,
                               dyad_dtl_mode_t mode,
                               dyad_dtl_comm_mode_t comm_mode,
                               bool debug)
{
    DYAD_C_FUNCTION_START ();

    // dyad_rc_t rc = DYAD_RC_OK;
    dyad_dtl_margo_t* margo_handle = NULL;

    ctx->dtl_handle->private_dtl.margo_dtl_handle = malloc (sizeof (struct dyad_dtl_margo));
    if (ctx->dtl_handle->private_dtl.margo_dtl_handle == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate internal Margo DTL context\n");
        DYAD_C_FUNCTION_END ();
        return DYAD_RC_SYSFAIL;
    }

    margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;
    margo_handle->h = (flux_t*)ctx->h;  // flux handle
    margo_handle->debug = debug;
    margo_handle->recv_ready = 0;

    // the underlying network protocol
    // to use for the margo dtl.
    // TODO: currently hardcoded.
    char margo_na_protocol[] = "ofi+tcp";  // works everywhere
    // char margo_na_protocol[] = "ofi+verbs";  // best for infiniband
    // char margo_na_protocol[] = "ofi+cxi";    // bset for HPC slingshot
    // producer (FLUX broker)
    // essentially the margo client
    if (comm_mode == DYAD_COMM_SEND) {
        margo_handle->mid = margo_init (margo_na_protocol, MARGO_CLIENT_MODE, 0, -1);
        margo_handle->sendrecv_rpc_id = MARGO_REGISTER (margo_handle->mid,
                                                        "data_ready_rpc",
                                                        margo_rpc_in_t,
                                                        margo_rpc_out_t,
                                                        NULL);
    }
    // consumer (dyad client c wrapper)
    // essentially the margo server
    if (comm_mode == DYAD_COMM_RECV) {
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
        margo_handle->sendrecv_rpc_id = MARGO_REGISTER (margo_handle->mid,
                                                        "data_ready_rpc",
                                                        margo_rpc_in_t,
                                                        margo_rpc_out_t,
                                                        data_ready_rpc);
        margo_register_data (margo_handle->mid, margo_handle->sendrecv_rpc_id, margo_handle, NULL);
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
    }
    if (comm_mode == DYAD_COMM_RECV) {
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

/* Packing an RPC message in a json string */
dyad_rc_t dyad_dtl_margo_rpc_pack (const dyad_ctx_t* ctx,
                                   const char* restrict upath,
                                   uint32_t producer_rank,
                                   json_t** restrict packed_obj)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;

    dyad_dtl_margo_t* margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    // send my address (me as consumer and margo server)
    char addr_str[128];
    size_t addr_str_size = 128;
    margo_addr_to_string (margo_handle->mid, addr_str, &addr_str_size, margo_handle->local_addr);

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

dyad_rc_t dyad_dtl_margo_rpc_unpack (const dyad_ctx_t* ctx, const flux_msg_t* msg, char** upath)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;

    uint64_t tag_prod = 0;
    uint64_t pid = 0;
    char* addr_str = NULL;
    size_t addr_str_size = 0;
    int errcode;

    dyad_dtl_margo_t* margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

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

dyad_rc_t dyad_dtl_margo_rpc_respond (const dyad_ctx_t* ctx, const flux_msg_t* orig_msg)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_margo_rpc_recv_response (const dyad_ctx_t* ctx, flux_future_t* f)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_margo_establish_connection (const dyad_ctx_t* ctx)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_C_FUNCTION_END ();
    return rc;
}

/* provider (now flux broker) calls send */
dyad_rc_t dyad_dtl_margo_send (const dyad_ctx_t* ctx, void* buf, size_t buflen)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;

    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo_send is called, buflen: %ld.", buflen);
    dyad_dtl_margo_t* margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    hg_size_t segment_sizes[1] = {buflen};
    void* segment_ptrs[1] = {buf};

    // Register my local data
    // which will be pulled by the consumer
    hg_bulk_t local_bulk;
    margo_bulk_create (margo_handle->mid,
                       1,
                       segment_ptrs,
                       segment_sizes,
                       HG_BULK_READ_ONLY,
                       &local_bulk);

    margo_rpc_in_t args;
    args.n = buflen;
    args.bulk = local_bulk;

    // send a message to the consuer, notifying
    // it that my data is ready
    hg_handle_t h;
    margo_create (margo_handle->mid, margo_handle->remote_addr, margo_handle->sendrecv_rpc_id, &h);
    margo_forward (h, &args);

    margo_rpc_out_t resp;
    margo_get_output (h, &resp);
    margo_free_output (h, &resp);
    margo_destroy (h);

    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo_send (buflen=%lu) completed.", buflen);

    DYAD_C_FUNCTION_END ();
    return rc;
}

/* consumer calls recv (which recvs from flux broker) */
dyad_rc_t dyad_dtl_margo_recv (const dyad_ctx_t* ctx, void** buf, size_t* buflen)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo_recv is called, waiting for data.");

    dyad_dtl_margo_t* margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    while (!margo_handle->recv_ready) {
        usleep (100);
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
    margo_handle->recv_ready = 0;

    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_margo_close_connection (const dyad_ctx_t* ctx)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    // dyad_dtl_margo_t* margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;
    // dyad_dtl_comm_mode_t comm_mode = dtl_handle->comm_mode;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_margo_finalize (const dyad_ctx_t* ctx)
{
    DYAD_C_FUNCTION_START ();

    dyad_dtl_margo_t* margo_handle;
    dyad_rc_t rc = DYAD_RC_OK;

    if (ctx->dtl_handle == NULL || ctx->dtl_handle->private_dtl.margo_dtl_handle == NULL) {
        rc = DYAD_RC_OK;
        goto dtl_margo_finalize_finish;
    }

    margo_handle = ctx->dtl_handle->private_dtl.margo_dtl_handle;

    margo_addr_free (margo_handle->mid, margo_handle->local_addr);
    if (margo_handle->remote_addr != NULL)
        margo_addr_free (margo_handle->mid, margo_handle->remote_addr);
    margo_finalize (margo_handle->mid);
    free (margo_handle);
    ctx->dtl_handle->private_dtl.margo_dtl_handle = NULL;

    DYAD_LOG_DEBUG (ctx, "[MARGO DTL] margo dtl finalized");

dtl_margo_finalize_finish:;
    DYAD_C_FUNCTION_END ();
    return rc;
}
