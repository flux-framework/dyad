#include "dyad_dtl_impl.h"

#include "flux_dtl.h"

#if DYAD_ENABLE_UCX
#include "ucx_dtl.h"
#endif

dyad_rc_t dyad_dtl_init (dyad_dtl_t **dtl_handle,
                         dyad_dtl_mode_t mode,
                         flux_t *h,
                         bool debug)
{
    dyad_rc_t rc = DYAD_RC_OK;
    *dtl_handle = malloc (sizeof (struct dyad_dtl));
    if (*dtl_handle == NULL) {
        return DYAD_RC_SYSFAIL;
    }
    (*dtl_handle)->mode = mode;
#if DYAD_ENABLE_UCX
    if (mode == DYAD_DTL_UCX) {
        rc = dyad_dtl_ucx_init (*dtl_handle, mode, h, debug);
        if (DYAD_IS_ERROR (rc)) {
            return rc;
        }
        return DYAD_RC_OK;
    } else if (mode == DYAD_DTL_FLUX_RPC) {
#else
    if (mode == DYAD_DTL_FLUX_RPC) {
#endif
        rc = dyad_dtl_flux_init (*dtl_handle, mode, h, debug);
        if (DYAD_IS_ERROR (rc)) {
            return rc;
        }
        return DYAD_RC_OK;
    }
    return DYAD_RC_BADDTLMODE;
}

dyad_rc_t dyad_dtl_finalize (dyad_dtl_t **dtl_handle)
{
    dyad_rc_t rc = DYAD_RC_OK;
    if (dtl_handle == NULL || *dtl_handle == NULL)
        // We should only reach this line if the user has passed
        // in an already-finalized DTL handle. In that case,
        // this function should be treated as a no-op, and we
        // should return DYAD_RC_OK to indicate no error has occured
        return DYAD_RC_OK;
#if DYAD_ENABLE_UCX
    if ((*dtl_handle)->mode == DYAD_DTL_UCX) {
        if ((*dtl_handle)->private.ucx_dtl_handle != NULL) {
            rc = dyad_dtl_ucx_finalize (dtl_handle);
            if (DYAD_IS_ERROR (rc)) {
                return rc;
            }
        }
    } else if ((*dtl_handle)->mode == DYAD_DTL_FLUX_RPC) {
#else
    if ((*dtl_handle)->mode == DYAD_DTL_FLUX_RPC) {
#endif
        if ((*dtl_handle)->private.flux_dtl_handle != NULL) {
            rc = dyad_dtl_flux_finalize (dtl_handle);
            if (DYAD_IS_ERROR (rc)) {
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
