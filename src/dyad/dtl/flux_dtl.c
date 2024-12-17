#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <unistd.h>  // sysconf

#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/dtl/flux_dtl.h>

dyad_rc_t dyad_dtl_flux_init (const dyad_ctx_t* ctx,
                              dyad_dtl_mode_t mode,
                              dyad_dtl_comm_mode_t comm_mode,
                              bool debug)
{
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_C_FUNCTION_START ();
    ctx->dtl_handle->private_dtl.flux_dtl_handle = malloc (sizeof (struct dyad_dtl_flux));
    if (ctx->dtl_handle->private_dtl.flux_dtl_handle == NULL) {
        DYAD_LOG_ERROR (ctx, "Cannot allocate the Flux DTL handle");
        rc = DYAD_RC_SYSFAIL;
        goto dtl_flux_init_region_finish;
    }
    ctx->dtl_handle->private_dtl.flux_dtl_handle->h = (flux_t*)ctx->h;
    ctx->dtl_handle->private_dtl.flux_dtl_handle->comm_mode = comm_mode;
    ctx->dtl_handle->private_dtl.flux_dtl_handle->debug = debug;
    ctx->dtl_handle->private_dtl.flux_dtl_handle->f = NULL;
    ctx->dtl_handle->private_dtl.flux_dtl_handle->msg = NULL;

    ctx->dtl_handle->rpc_pack = dyad_dtl_flux_rpc_pack;
    ctx->dtl_handle->rpc_unpack = dyad_dtl_flux_rpc_unpack;
    ctx->dtl_handle->rpc_respond = dyad_dtl_flux_rpc_respond;
    ctx->dtl_handle->rpc_recv_response = dyad_dtl_flux_rpc_recv_response;
    ctx->dtl_handle->get_buffer = dyad_dtl_flux_get_buffer;
    ctx->dtl_handle->return_buffer = dyad_dtl_flux_return_buffer;
    ctx->dtl_handle->establish_connection = dyad_dtl_flux_establish_connection;
    ctx->dtl_handle->send = dyad_dtl_flux_send;
    ctx->dtl_handle->recv = dyad_dtl_flux_recv;
    ctx->dtl_handle->close_connection = dyad_dtl_flux_close_connection;

dtl_flux_init_region_finish:
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_flux_rpc_pack (const dyad_ctx_t* ctx,
                                  const char* restrict upath,
                                  uint32_t producer_rank,
                                  json_t** restrict packed_obj)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("upath", upath);
    DYAD_C_FUNCTION_UPDATE_INT ("producer_rank", producer_rank);
    dyad_rc_t rc = DYAD_RC_OK;
    *packed_obj = json_pack ("{s:s}", "upath", upath);
    if (*packed_obj == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not pack upath for Flux DTL");
        rc = DYAD_RC_BADPACK;
        goto dtl_flux_rpc_pack;
    }
dtl_flux_rpc_pack:
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_flux_rpc_unpack (const dyad_ctx_t* ctx, const flux_msg_t* msg, char** upath)
{
    DYAD_C_FUNCTION_START ();
    int rc = 0;
    dyad_rc_t dyad_rc = DYAD_RC_OK;
    rc = flux_request_unpack (msg, NULL, "{s:s}", "upath", upath);
    if (FLUX_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Could not unpack Flux message from consumer");
        // TODO create new RC for this
        dyad_rc = DYAD_RC_BADUNPACK;
        goto dtl_flux_rpc_unpack_region_finish;
    }
    ctx->dtl_handle->private_dtl.flux_dtl_handle->msg = (flux_msg_t*)msg;
    dyad_rc = DYAD_RC_OK;
    DYAD_C_FUNCTION_UPDATE_STR ("upath", *upath);
dtl_flux_rpc_unpack_region_finish:
    DYAD_C_FUNCTION_END ();
    return dyad_rc;
}

dyad_rc_t dyad_dtl_flux_rpc_respond (const dyad_ctx_t* ctx, const flux_msg_t* orig_msg)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_rpc_recv_response (const dyad_ctx_t* ctx, flux_future_t* f)
{
    DYAD_C_FUNCTION_START ();
    ctx->dtl_handle->private_dtl.flux_dtl_handle->f = f;
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_get_buffer (const dyad_ctx_t* ctx, size_t data_size, void** data_buf)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;

    if (data_buf == NULL || *data_buf != NULL) {
        rc = DYAD_RC_BADBUF;
        goto flux_get_buf_done;
    }
#if 0
    *data_buf = malloc (data_size);
    if (*data_buf == NULL) {
        rc = DYAD_RC_SYSFAIL;
        goto flux_get_buf_done;
    }
#else
    rc = posix_memalign (data_buf, sysconf (_SC_PAGESIZE), data_size);
    if (rc != 0 || *data_buf == NULL) {
        rc = DYAD_RC_SYSFAIL;
        goto flux_get_buf_done;
    }
#endif
    rc = DYAD_RC_OK;

flux_get_buf_done:
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_flux_return_buffer (const dyad_ctx_t* ctx, void** data_buf)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    if (data_buf == NULL || *data_buf == NULL) {
        rc = DYAD_RC_BADBUF;
        goto flux_ret_buf_done;
    }
    free (*data_buf);
    rc = DYAD_RC_OK;

flux_ret_buf_done:
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_flux_establish_connection (const dyad_ctx_t* ctx)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_send (const dyad_ctx_t* ctx, void* buf, size_t buflen)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t dyad_rc = DYAD_RC_OK;
    int rc = 0;
    DYAD_LOG_INFO (ctx, "Send data to consumer using a Flux RPC response");
    rc = flux_respond_raw (ctx->dtl_handle->private_dtl.flux_dtl_handle->h,
                           ctx->dtl_handle->private_dtl.flux_dtl_handle->msg,
                           buf,
                           (int)buflen);
    if (FLUX_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx,
                        "Could not send Flux RPC response containing file "
                        "contents");
        dyad_rc = DYAD_RC_FLUXFAIL;
        goto dtl_flux_send_region_finish;
    }
    if (ctx->dtl_handle->private_dtl.flux_dtl_handle->debug) {
        DYAD_LOG_INFO (ctx, "Successfully sent file contents to consumer");
    }
    dyad_rc = DYAD_RC_OK;
dtl_flux_send_region_finish:
    DYAD_C_FUNCTION_UPDATE_INT ("buflen", buflen);
    DYAD_C_FUNCTION_END ();
    return dyad_rc;
}

dyad_rc_t dyad_dtl_flux_recv (const dyad_ctx_t* ctx, void** buf, size_t* buflen)
{
    DYAD_C_FUNCTION_START ();
    int rc = 0;
    dyad_rc_t dyad_rc = DYAD_RC_OK;
    errno = 0;
    dyad_dtl_flux_t* dtl_handle = ctx->dtl_handle->private_dtl.flux_dtl_handle;
    DYAD_LOG_INFO (ctx, "Get file contents from module using Flux RPC");
    if (dtl_handle->f == NULL) {
        DYAD_LOG_ERROR (ctx, "Cannot get data using RPC without a Flux future");
        // TODO create new RC for this
        dyad_rc = DYAD_RC_FLUXFAIL;
        goto finish_recv;
    }
    void* tmp_buf;
    int tmp_buflen = 0;
    rc = flux_rpc_get_raw (dtl_handle->f, (const void**)&tmp_buf, (int*)&tmp_buflen);
    if (FLUX_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Could not get file data from Flux RPC");
        if (errno == ENODATA)
            dyad_rc = DYAD_RC_RPC_FINISHED;
        else
            dyad_rc = DYAD_RC_BADRPC;
        goto finish_recv;
    }
    *buflen = (size_t)tmp_buflen;
    dyad_rc = ctx->dtl_handle->get_buffer (ctx, *buflen, buf);
    if (DYAD_IS_ERROR (dyad_rc)) {
        *buf = NULL;
        *buflen = 0;
        goto finish_recv;
    }
    memcpy (*buf, tmp_buf, *buflen);
    dyad_rc = DYAD_RC_OK;
finish_recv:
    if (dtl_handle->f != NULL)
        flux_future_reset (dtl_handle->f);
    DYAD_C_FUNCTION_UPDATE_INT ("tmp_buflen", tmp_buflen);
    DYAD_C_FUNCTION_END ();
    return dyad_rc;
}

dyad_rc_t dyad_dtl_flux_close_connection (const dyad_ctx_t* ctx)
{
    DYAD_C_FUNCTION_START ();
    if (ctx->dtl_handle->private_dtl.flux_dtl_handle->f != NULL)
        ctx->dtl_handle->private_dtl.flux_dtl_handle->f = NULL;
    if (ctx->dtl_handle->private_dtl.flux_dtl_handle->msg != NULL)
        ctx->dtl_handle->private_dtl.flux_dtl_handle->msg = NULL;
    DYAD_C_FUNCTION_END ();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_finalize (const dyad_ctx_t* ctx)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    if (ctx->dtl_handle == NULL) {
        goto dtl_flux_finalize_done;
    }
    ctx->dtl_handle->private_dtl.flux_dtl_handle->h = NULL;
    ctx->dtl_handle->private_dtl.flux_dtl_handle->f = NULL;
    ctx->dtl_handle->private_dtl.flux_dtl_handle->msg = NULL;
    free (ctx->dtl_handle->private_dtl.flux_dtl_handle);
    ctx->dtl_handle->private_dtl.flux_dtl_handle = NULL;
dtl_flux_finalize_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}
