#include "dyad_dtl_impl.h"

#include "ucx_dtl.h"
#include "flux_dtl.h"

dyad_rc_t dyad_dtl_init(dyad_dtl_t **dtl_handle,
        dyad_dtl_mode_t mode, flux_t *h, bool debug)
{
    dyad_rc_t rc = DYAD_RC_OK;
    *dtl_handle = malloc (sizeof (struct dyad_dtl));
    if (*dtl_handle == NULL) {
        return DYAD_RC_SYSFAIL;
    }
    (*dtl_handle)->mode = mode;
    if (mode == DYAD_DTL_UCX) {
        rc = dyad_dtl_ucx_init (
            &((*dtl_handle)->private.ucx_dtl_handle),
            mode,
            h,
            debug
        );
        if (DYAD_IS_ERROR(rc)) {
            return rc;
        }
        (*dtl_handle)->rpc_pack = dyad_dtl_ucx_rpc_pack;
        (*dtl_handle)->rpc_unpack = dyad_dtl_ucx_rpc_unpack;
        (*dtl_handle)->rpc_respond = dyad_dtl_ucx_rpc_respond;
        (*dtl_handle)->rpc_recv_response = dyad_dtl_ucx_rpc_recv_response;
        (*dtl_handle)->establish_connection = dyad_dtl_ucx_establish_connection;
        (*dtl_handle)->send = dyad_dtl_ucx_send;
        (*dtl_handle)->recv = dyad_dtl_ucx_recv;
        (*dtl_handle)->close_connection = dyad_dtl_ucx_close_connection;
        return DYAD_RC_OK;
    } else if (mode == DYAD_DTL_FLUX_RPC) {
        rc = dyad_dtl_flux_init (
            &((*dtl_handle)->private.flux_dtl_handle),
            mode,
            h,
            debug
        );
        if (DYAD_IS_ERROR(rc)) {
            return rc;
        }
        (*dtl_handle)->rpc_pack = dyad_dtl_flux_rpc_pack;
        (*dtl_handle)->rpc_unpack = dyad_dtl_flux_rpc_unpack;
        (*dtl_handle)->rpc_respond = dyad_dtl_flux_rpc_respond;
        (*dtl_handle)->rpc_recv_response = dyad_dtl_flux_rpc_recv_response;
        (*dtl_handle)->establish_connection = dyad_dtl_flux_establish_connection;
        (*dtl_handle)->send = dyad_dtl_flux_send;
        (*dtl_handle)->recv = dyad_dtl_flux_recv;
        (*dtl_handle)->close_connection = dyad_dtl_flux_close_connection;
        return DYAD_RC_OK;
    }
    return DYAD_RC_BADDTLMODE;
}

dyad_rc_t dyad_dtl_finalize(dyad_dtl_t **dtl_handle)
{
    dyad_rc_t rc = DYAD_RC_OK;
    if (dtl_handle == NULL || *dtl_handle == NULL)
        return DYAD_RC_OK;
    if ((*dtl_handle)->mode == DYAD_DTL_UCX) {
        if ((*dtl_handle)->private.ucx_dtl_handle != NULL) {
            rc = dyad_dtl_ucx_finalize (dtl_handle);
            if (DYAD_IS_ERROR(rc)) {
                return rc;
            }
        }
    } else if ((*dtl_handle)->mode == DYAD_DTL_FLUX_RPC) {
        if ((*dtl_handle)->private.flux_dtl_handle != NULL) {
            rc = dyad_dtl_flux_finalize (dtl_handle);
            if (DYAD_IS_ERROR(rc)) {
                return rc;
            }
        }
    } else {
        return DYAD_RC_BADDTLMODE;
    }
    free (*dtl_handle);
    *dtl_handle = NULL;
    return DYAD_RC_OK;
}
