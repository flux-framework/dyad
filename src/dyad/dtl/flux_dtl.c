#include <dyad/dtl/flux_dtl.h>

dyad_rc_t dyad_dtl_flux_init (dyad_dtl_t* self,
                              dyad_dtl_mode_t mode,
                              flux_t* h,
                              bool debug)
{
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_PERF_REGION_BEGIN (self->perf_handle, "dyad_dtl_flux_init");
    self->private.flux_dtl_handle = malloc (sizeof (struct dyad_dtl_flux));
    if (self->private.flux_dtl_handle == NULL) {
        FLUX_LOG_ERR (h, "Cannot allocate the Flux DTL handle\n");
        rc = DYAD_RC_SYSFAIL;
        goto dtl_flux_init_region_finish;
    }
    self->private.flux_dtl_handle->h = h;
    self->private.flux_dtl_handle->debug = debug;
    self->private.flux_dtl_handle->f = NULL;
    self->private.flux_dtl_handle->msg = NULL;

    self->rpc_pack = dyad_dtl_flux_rpc_pack;
    self->rpc_unpack = dyad_dtl_flux_rpc_unpack;
    self->rpc_respond = dyad_dtl_flux_rpc_respond;
    self->rpc_recv_response = dyad_dtl_flux_rpc_recv_response;
    self->establish_connection = dyad_dtl_flux_establish_connection;
    self->send = dyad_dtl_flux_send;
    self->recv = dyad_dtl_flux_recv;
    self->close_connection = dyad_dtl_flux_close_connection;

    rc = DYAD_RC_OK;
dtl_flux_init_region_finish:
    DYAD_PERF_REGION_END (self->perf_handle, "dyad_dtl_flux_init");
    return rc;
}

dyad_rc_t dyad_dtl_flux_rpc_pack (dyad_dtl_t* restrict self,
                                  const char* restrict upath,
                                  uint32_t producer_rank,
                                  json_t** restrict packed_obj)
{
    dyad_rc_t rc = DYAD_RC_OK;
    DYAD_PERF_REGION_BEGIN (self->perf_handle, "dyad_dtl_flux_rpc_pack");
    dyad_dtl_flux_t* dtl_handle = self->private.flux_dtl_handle;
    *packed_obj = json_pack ("{s:s}", "upath", upath);
    if (*packed_obj == NULL) {
        FLUX_LOG_ERR (dtl_handle->h, "Could not pack upath for Flux DTL\n");
        rc = DYAD_RC_BADPACK;
        goto dtl_flux_rpc_pack;
    }
    rc = DYAD_RC_OK;
dtl_flux_rpc_pack:
    DYAD_PERF_REGION_END (self->perf_handle, "dyad_dtl_flux_rpc_pack");
    return rc;
}

dyad_rc_t dyad_dtl_flux_rpc_unpack (dyad_dtl_t* self, const flux_msg_t* msg, char** upath)
{
    int rc = 0;
    dyad_rc_t dyad_rc = DYAD_RC_OK;
    DYAD_PERF_REGION_BEGIN (self->perf_handle, "dyad_dtl_flux_rpc_unpack");
    rc = flux_request_unpack (msg, NULL, "{s:s}", "upath", upath);
    if (FLUX_IS_ERROR (rc)) {
        FLUX_LOG_ERR (self->private.flux_dtl_handle->h,
                      "Could not unpack Flux message from consumer\n");
        // TODO create new RC for this
        dyad_rc = DYAD_RC_BADUNPACK;
        goto dtl_flux_rpc_unpack_region_finish;
    }
    self->private.flux_dtl_handle->msg = (flux_msg_t*)msg;
    dyad_rc = DYAD_RC_OK;
dtl_flux_rpc_unpack_region_finish:
    DYAD_PERF_REGION_END (self->perf_handle, "dyad_dtl_flux_rpc_unpack");
    return dyad_rc;
}

dyad_rc_t dyad_dtl_flux_rpc_respond (dyad_dtl_t* self, const flux_msg_t* orig_msg)
{
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_rpc_recv_response (dyad_dtl_t* self, flux_future_t* f)
{
    self->private.flux_dtl_handle->f = f;
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_establish_connection (dyad_dtl_t* self,
                                              dyad_dtl_comm_mode_t comm_mode)
{
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_send (dyad_dtl_t* self, void* buf, size_t buflen)
{
    int rc = 0;
    dyad_rc_t dyad_rc = DYAD_RC_OK;
    DYAD_PERF_REGION_BEGIN (self->perf_handle, "dyad_dtl_flux_send");
    FLUX_LOG_INFO (self->private.flux_dtl_handle->h,
                   "Send data to consumer using a Flux RPC response");
    rc = flux_respond_raw (self->private.flux_dtl_handle->h,
                           self->private.flux_dtl_handle->msg,
                           buf,
                           (int)buflen);
    if (FLUX_IS_ERROR (rc)) {
        FLUX_LOG_ERR (self->private.flux_dtl_handle->h,
                      "Could not send Flux RPC response containing file "
                      "contents\n");
        dyad_rc = DYAD_RC_FLUXFAIL;
        goto dtl_flux_send_region_finish;
    }
    if (self->private.flux_dtl_handle->debug) {
        FLUX_LOG_INFO (self->private.flux_dtl_handle->h,
                       "Successfully sent file contents to consumer\n");
    }
    dyad_rc = DYAD_RC_OK;
dtl_flux_send_region_finish:
    DYAD_PERF_REGION_END (self->perf_handle, "dyad_dtl_flux_send");
    return dyad_rc;
}

dyad_rc_t dyad_dtl_flux_recv (dyad_dtl_t* self, void** buf, size_t* buflen)
{
    int rc = 0;
    dyad_rc_t dyad_rc = DYAD_RC_OK;
    errno = 0;
    dyad_dtl_flux_t* dtl_handle = self->private.flux_dtl_handle;
    DYAD_PERF_REGION_BEGIN (self->perf_handle, "dyad_dtl_flux_recv");
    FLUX_LOG_INFO (dtl_handle->h, "Get file contents from module using Flux RPC\n");
    if (dtl_handle->f == NULL) {
        FLUX_LOG_ERR (dtl_handle->h, "Cannot get data using RPC without a Flux future\n");
        // TODO create new RC for this
        dyad_rc = DYAD_RC_FLUXFAIL;
        goto finish_recv;
    }
    void* tmp_buf;
    size_t tmp_buflen;
    rc = flux_rpc_get_raw (dtl_handle->f, (const void**)&tmp_buf, (int*)&tmp_buflen);
    if (FLUX_IS_ERROR (rc)) {
        FLUX_LOG_ERR (dtl_handle->h, "Could not get file data from Flux RPC");
        if (errno == ENODATA)
            dyad_rc = DYAD_RC_RPC_FINISHED;
        else
            dyad_rc = DYAD_RC_BADRPC;
        goto finish_recv;
    }
    *buflen = tmp_buflen;
    if (*buf == NULL) {
        *buf = malloc (tmp_buflen);
        if (*buf == NULL) {
            FLUX_LOG_ERR (dtl_handle->h, "Could not allocate space for file buffer");
            dyad_rc = DYAD_RC_SYSFAIL;
            goto finish_recv;
        }
    }
    memcpy (*buf, tmp_buf, tmp_buflen);
    dyad_rc = DYAD_RC_OK;
finish_recv:
    if (dtl_handle->f != NULL)
        flux_future_reset (dtl_handle->f);
    DYAD_PERF_REGION_END (self->perf_handle, "dyad_dtl_flux_recv");
    return dyad_rc;
}

dyad_rc_t dyad_dtl_flux_close_connection (dyad_dtl_t* self)
{
    if (self->private.flux_dtl_handle->f != NULL)
        self->private.flux_dtl_handle->f = NULL;
    if (self->private.flux_dtl_handle->msg != NULL)
        self->private.flux_dtl_handle->msg = NULL;
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_finalize (dyad_dtl_t** self)
{
    if (self == NULL || *self == NULL) {
        return DYAD_RC_OK;
    }
    dyad_perf_t* perf_handle = (*self)->perf_handle;
    DYAD_PERF_REGION_BEGIN (perf_handle, "dyad_dtl_flux_finalize");
    (*self)->private.flux_dtl_handle->h = NULL;
    (*self)->private.flux_dtl_handle->f = NULL;
    (*self)->private.flux_dtl_handle->msg = NULL;
    free ((*self)->private.flux_dtl_handle);
    (*self)->private.flux_dtl_handle = NULL;
    DYAD_PERF_REGION_END (perf_handle, "dyad_dtl_flux_finalize");
    return DYAD_RC_OK;
}