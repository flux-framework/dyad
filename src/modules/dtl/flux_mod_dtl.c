#include "flux_mod_dtl.h"

int dyad_mod_flux_dtl_init(flux_t *h, bool debug,
        dyad_mod_flux_dtl_t **dtl_handle)
{
    *dtl_handle = malloc(sizeof(struct dyad_mod_flux_dtl));
    if (*dtl_handle == NULL)
    {
        FLUX_LOG_ERR(h, "Cannot allocate a context for the Flux RPC DTL\n");
        return -1;
    }
    (*dtl_handle)->h = h;
    (*dtl_handle)->debug = debug;
    (*dtl_handle)->msg = NULL;
    return 0;
}

int dyad_mod_flux_dtl_rpc_unpack(dyad_mod_flux_dtl_t *dtl_handle,
        const flux_msg_t *packed_obj, char **upath)
{
    int errcode = flux_request_unpack(
        packed_obj,
        NULL,
        "{s:s}",
        "upath",
        upath
    );
    if (errcode < 0)
    {
        FLUX_LOG_ERR(dtl_handle->h, "Could not unpack Flux message from consumer!\n");
        return -1;
    }
    // Save the flux_msg_t object here instead of dyad_mod_flux_dtl_rpc_respond
    // to increase the odds that the compiler will optimize rpc_respond away
    dtl_handle->msg = packed_obj;
    return 0;
}

int dyad_mod_flux_dtl_rpc_respond (dyad_mod_flux_dtl_t *dtl_handle,
        const flux_msg_t *orig_msg)
{
    return 0;
}

int dyad_mod_flux_dtl_establish_connection(dyad_mod_flux_dtl_t *dtl_handle)
{
    return 0;
}

int dyad_mod_flux_dtl_send(dyad_mod_flux_dtl_t *dtl_handle, void *buf, size_t buflen)
{
    FLUX_LOG_INFO (dtl_handle->h, "Send data to consumer using a Flux RPC response");
    int errcode = flux_respond_raw(dtl_handle->h, dtl_handle->msg, buf, (int)buflen);
    if (errcode < 0)
    {
        FLUX_LOG_ERR(dtl_handle->h, "Could not send Flux RPC response containing file contents!\n");
        return -1;
    }
    if (dtl_handle->debug)
    {
        FLUX_LOG_INFO(dtl_handle->h, "Successfully sent file contents to consumer!\n");
    }
    return 0;
}

int dyad_mod_flux_dtl_close_connection(dyad_mod_flux_dtl_t *dtl_handle)
{
    if (dtl_handle != NULL)
    {
        dtl_handle->msg = NULL;
    }
    return 0;
}

int dyad_mod_flux_dtl_finalize(dyad_mod_flux_dtl_t *dtl_handle)
{
    if (dtl_handle != NULL)
    {
        free(dtl_handle);
    }
    return 0;
}
