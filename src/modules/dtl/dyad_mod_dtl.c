#include "dyad_mod_dtl.h"

#include "ucx_mod_dtl.h"
#include "flux_mod_dtl.h"

struct dyad_mod_dtl {
    dyad_mod_dtl_mode_t mode;
    flux_t *h;
    void *real_handle;
};

int dyad_mod_dtl_init(dyad_mod_dtl_mode_t mode,
        flux_t *h, bool debug,
        dyad_mod_dtl_t **dtl_handle)
{
    *dtl_handle = malloc(sizeof(struct dyad_mod_dtl));
    if (*dtl_handle == NULL)
    {
        FLUX_LOG_ERR (h, "Could not allocate a dyad_mode_dtl_t object\n");
        return -1;
    }
    (*dtl_handle)->mode = mode;
    (*dtl_handle)->h = h;
    if (mode == DYAD_DTL_UCX)
    {
        FLUX_LOG_INFO (h, "Initializing UCX DTL!\n");
        return dyad_mod_ucx_dtl_init(
            h,
            debug,
            (dyad_mod_ucx_dtl_t**)&(*dtl_handle)->real_handle
        );
    }
    if (mode == DYAD_DTL_FLUX_RPC)
    {
        FLUX_LOG_INFO (h, "Initializing Flux RPC DTL!\n");
        return dyad_mod_flux_dtl_init(
            h,
            debug,
            (dyad_mod_flux_dtl_t**)&(*dtl_handle)->real_handle
        );
    }
    FLUX_LOG_ERR (h, "Invalid DYAD DTL mode: %d\n", (int) mode);
    return -1;
}

int dyad_mod_dtl_rpc_unpack(dyad_mod_dtl_t *dtl_handle,
        const flux_msg_t *packed_obj, char **upath)
{
    if (dtl_handle->mode == DYAD_DTL_UCX)
    {
        return dyad_mod_ucx_dtl_rpc_unpack(
            (dyad_mod_ucx_dtl_t*)dtl_handle->real_handle,
            packed_obj,
            upath
        );
    }
    if (dtl_handle->mode == DYAD_DTL_FLUX_RPC)
    {
        return dyad_mod_flux_dtl_rpc_unpack(
            (dyad_mod_flux_dtl_t*)dtl_handle->real_handle,
            packed_obj,
            upath
        );
    }
    FLUX_LOG_ERR (dtl_handle->h, "Invalid DYAD DTL mode: %d\n", (int) dtl_handle->mode);
    return -1;
}

int dyad_mod_dtl_rpc_respond(dyad_mod_dtl_t *dtl_handle, const flux_msg_t *orig_msg)
{
    if (dtl_handle->mode == DYAD_DTL_UCX)
    {
        return dyad_mod_ucx_dtl_rpc_respond(
            (dyad_mod_ucx_dtl_t*)dtl_handle->real_handle,
            orig_msg
        );
    }
    if (dtl_handle->mode == DYAD_DTL_FLUX_RPC)
    {
        return dyad_mod_flux_dtl_rpc_respond(
            (dyad_mod_flux_dtl_t*)dtl_handle->real_handle,
            orig_msg
        );
    }
    FLUX_LOG_ERR (dtl_handle->h, "Invalid DYAD DTL mode: %d\n", (int) dtl_handle->mode);
    return -1;
}

int dyad_mod_dtl_establish_connection(dyad_mod_dtl_t *dtl_handle)
{
    if (dtl_handle->mode == DYAD_DTL_UCX)
    {
        return dyad_mod_ucx_dtl_establish_connection(
            (dyad_mod_ucx_dtl_t*)dtl_handle->real_handle
        );
    }
    if (dtl_handle->mode == DYAD_DTL_FLUX_RPC)
    {
        return dyad_mod_flux_dtl_establish_connection(
            (dyad_mod_flux_dtl_t*)dtl_handle->real_handle
        );
    }
    FLUX_LOG_ERR (dtl_handle->h, "Invalid DYAD DTL mode: %d\n", (int) dtl_handle->mode);
    return -1;
}

int dyad_mod_dtl_send(dyad_mod_dtl_t *dtl_handle, void *buf, size_t buflen)
{
    if (dtl_handle->mode == DYAD_DTL_UCX)
    {
        return dyad_mod_ucx_dtl_send(
            (dyad_mod_ucx_dtl_t*)dtl_handle->real_handle,
            buf,
            buflen
        );
    }
    if (dtl_handle->mode == DYAD_DTL_FLUX_RPC)
    {
        return dyad_mod_flux_dtl_send(
            (dyad_mod_flux_dtl_t*)dtl_handle->real_handle,
            buf,
            buflen
        );
    }
    FLUX_LOG_ERR (dtl_handle->h, "Invalid DYAD DTL mode: %d\n", (int) dtl_handle->mode);
    return -1;
}

int dyad_mod_dtl_close_connection(dyad_mod_dtl_t *dtl_handle)
{
    if (dtl_handle->mode == DYAD_DTL_UCX)
    {
        return dyad_mod_ucx_dtl_close_connection(
            (dyad_mod_ucx_dtl_t*)dtl_handle->real_handle
        );
    }
    if (dtl_handle->mode == DYAD_DTL_FLUX_RPC)
    {
        return dyad_mod_flux_dtl_close_connection(
            (dyad_mod_flux_dtl_t*)dtl_handle->real_handle
        );
    }
    FLUX_LOG_ERR (dtl_handle->h, "Invalid DYAD DTL mode: %d\n", (int) dtl_handle->mode);
    return -1;
}

int dyad_mod_dtl_finalize(dyad_mod_dtl_t **dtl_handle)
{
    if (dtl_handle == NULL || *dtl_handle == NULL)
        return 0;
    dyad_mod_dtl_mode_t mode = (*dtl_handle)->mode;;
    flux_t *h = (*dtl_handle)->h;
    void* real_handle = (*dtl_handle)->real_handle;
    free(*dtl_handle);
    if (mode == DYAD_DTL_UCX)
    {
        return dyad_mod_ucx_dtl_finalize(
            (dyad_mod_ucx_dtl_t*)real_handle
        );
    }
    if (mode == DYAD_DTL_FLUX_RPC)
    {
        return dyad_mod_flux_dtl_finalize(
            (dyad_mod_flux_dtl_t*)real_handle
        );
    }
    FLUX_LOG_ERR (h, "Invalid DYAD DTL mode: %d\n", (int) mode);
    return -1;
}
