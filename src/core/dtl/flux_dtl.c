#include "flux_dtl.h"
#include "dyad_rc.h"

dyad_rc_t dyad_dtl_flux_init(flux_t *h, const char *kvs_namespace,
        bool debug, dyad_dtl_flux_t **dtl_handle)
{
    *dtl_handle = malloc(sizeof(struct dyad_dtl_flux));
    if (*dtl_handle == NULL)
    {
        FLUX_LOG_ERR (h, "Cannot allocate the DTL handle for Flux\n");
        return DYAD_RC_SYSFAIL;
    }
    (*dtl_handle)->h = h;
    (*dtl_handle)->kvs_namespace = kvs_namespace;
    (*dtl_handle)->f = NULL;
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_rpc_pack(dyad_dtl_flux_t *dtl_handle, const char *upath,
        uint32_t producer_rank, json_t **packed_obj)
{
    *packed_obj = json_pack(
        "{s:s}",
        "upath",
        upath
    );
    if (*packed_obj == NULL)
    {
        FLUX_LOG_ERR (dtl_handle->h, "Could not pack upath for Flux DTL\n");
        return DYAD_RC_BADPACK;
    }
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_recv_rpc_response(dyad_dtl_flux_t *dtl_handle,
        flux_future_t *f)
{
    dtl_handle->f = f;
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_establish_connection(dyad_dtl_flux_t *dtl_handle)
{
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_recv(dyad_dtl_flux_t *dtl_handle,
        void **buf, size_t *buflen)
{
    int rc = 0;
    void* tmp_buf = NULL;
    size_t tmp_buflen = 0;
    errno = 0;
    FLUX_LOG_INFO (dtl_handle->h, "Get file contents from module using RPC\n");
    rc = flux_rpc_get_raw(dtl_handle->f, (const void**) &tmp_buf, (int*) &tmp_buflen);
    if (rc < 0)
    {
        FLUX_LOG_ERR (dtl_handle->h, "Could not get file data from Flux RPC\n");
        if (errno == ENODATA)
            return DYAD_RC_RPC_FINISHED;
        return DYAD_RC_BADRPC;
    }
    *buflen = tmp_buflen;
    *buf = malloc(*buflen);
    memcpy(*buf, tmp_buf, *buflen);
    flux_future_reset (dtl_handle->f);
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_close_connection(dyad_dtl_flux_t *dtl_handle)
{
    dtl_handle->f = NULL;
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_flux_finalize(dyad_dtl_flux_t *dtl_handle)
{
    if (dtl_handle != NULL)
    {
        dtl_handle->h = NULL;
        dtl_handle->kvs_namespace = NULL;
        dtl_handle->f = NULL;
        free(dtl_handle);
        dtl_handle = NULL;
    }
    return DYAD_RC_OK;
}
