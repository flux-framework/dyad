#include "flux_dtl.h"

dyad_rc_t dyad_dtl_flux_init (dyad_dtl_t* dtl_handle,
                              dyad_dtl_mode_t mode,
                              flux_t* h,
                              bool debug)
{
    self->private.flux_dtl_handle = malloc (sizeof (struct dyad_dtl_flux));
    if (self->private.flux_dtl_handle == NULL) {
        FLUX_LOG_ERR (h, "Cannot allocate the Flux DTL handle\n");
        return DYAD_RC_SYSFAIL;
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

    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_rpc_pack (dyad_dtl_t* restrict self,
                                  const char* restrict upath,
                                  uint32_t producer_rank,
                                  json_t** restrict packed_obj)
{
    dyad_dtl_flux_t* dtl_handle = self->private.flux_dtl_handle;
    *packed_obj = json_pack ("{s:s}", "upath", upath);
    if (*packed_obj == NULL) {
        FLUX_LOG_ERR (dtl_handle->h, "Could not pack upath for Flux DTL\n");
        return DYAD_RC_BADPACK;
    }
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_rpc_unpack (dyad_dtl_t* self,
                                    const flux_msg_t* msg,
                                    char** upath)
{
    int errcode = 0;
    errcode = flux_request_unpack (msg, NULL, "{s:s}", "upath", upath);
    if (errcode < 0) {
        FLUX_LOG_ERR (self->private.flux_dtl_handle->h,
                      "Could not unpack Flux message from consumer\n");
        // TODO create new RC for this
        return -1;
    }
    self->private.flux_dtl_handle->msg = msg;
    return DYAD_RC_OK;
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
    int errcode = 0;
    FLUX_LOG_INFO (self->private.flux_dtl_handle->h,
                   "Send data to consumer using a Flux RPC response");
    errcode = flux_respond_raw (self->private.flux_dtl_handle->h,
                                self->private.flux_dtl_handle->msg,
                                buf,
                                (int)buflen);
    if (errcode < 0) {
        FLUX_LOG_ERR (self->private.flux_dtl_handle->h,
                      "Could not send Flux RPC response containing file "
                      "contents\n");
        return DYAD_RC_FLUXFAIL;
    }
    if (self->private.flux_dtl_handle->debug) {
        FLUX_LOG_INFO (self->private.flux_dtl_handle->h,
                       "Successfully sent file contents to consumer\n");
    }
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_recv (dyad_dtl_t* self, void** buf, size_t* buflen)
{
    int rc = 0;
    errno = 0;
    dyad_dtl_flux_t* dtl_handle = self->private.flux_dtl_handle;
    FLUX_LOG_INFO (dtl_handle->h, "Get file contents from module using Flux RPC\n");
    if (dtl_handle->f == NULL) {
        FLUX_LOG_ERR (dtl_handle->h,
                      "Cannot get data using RPC without a Flux future\n");
        // TODO create new RC for this
        return -1;
    }
    rc = flux_rpc_get_raw (dtl_handle->f, (const void**)buf, (int*)buflen);
    if (rc < 0) {
        FLUX_LOG_ERR (dtl_handle->h, "Could not get file data from Flux RPC\n");
        if (errno == ENODATA)
            return DYAD_RC_RPC_FINISHED;
        return DYAD_RC_BADRPC;
    }
    return DYAD_RC_OK;
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
    if (self == NULL || *self == NULL)
        return DYAD_RC_OK;
    (*self)->private.flux_dtl_handle->h = NULL;
    (*self)->private.flux_dtl_handle->f = NULL;
    (*self)->private.flux_dtl_handle->msg = NULL;
    free ((*self)->private.flux_dtl_handle);
    (*self)->private.flux_dtl_handle = NULL;
    return DYAD_RC_OK;
}