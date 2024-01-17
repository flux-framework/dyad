#include <dyad/dtl/dyad_dtl_impl.h>

#include <dyad/dtl/flux_dtl.h>

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
        rc = DYAD_RC_SYSFAIL;
        goto dtl_init_done;
    }
    (*dtl_handle)->mode = mode;
#if DYAD_ENABLE_UCX
    if (mode == DYAD_DTL_UCX) {
        rc = dyad_dtl_ucx_init (*dtl_handle, mode, h, debug);
        if (DYAD_IS_ERROR (rc)) {
            goto dtl_init_done;
        }
    } else if (mode == DYAD_DTL_FLUX_RPC) {
#else
    if (mode == DYAD_DTL_FLUX_RPC) {
#endif
        rc = dyad_dtl_flux_init (*dtl_handle, mode, h, debug);
        if (DYAD_IS_ERROR (rc)) {
            goto dtl_init_done;
        }
    } else {
        rc = DYAD_RC_BADDTLMODE;
        goto dtl_init_done;
    }
    rc = DYAD_RC_OK;

dtl_init_done:
    return rc;
}

dyad_rc_t dyad_dtl_finalize (dyad_dtl_t **dtl_handle)
{
    dyad_rc_t rc = DYAD_RC_OK;
    if (dtl_handle == NULL || *dtl_handle == NULL) {
        // We should only reach this line if the user has passed
        // in an already-finalized DTL handle. In that case,
        // this function should be treated as a no-op, and we
        // should return DYAD_RC_OK to indicate no error has occured
        rc = DYAD_RC_OK;
        goto dtl_finalize_done;
    }
#if DYAD_ENABLE_UCX
    if ((*dtl_handle)->mode == DYAD_DTL_UCX) {
        if ((*dtl_handle)->private.ucx_dtl_handle != NULL) {
            rc = dyad_dtl_ucx_finalize (dtl_handle);
            if (DYAD_IS_ERROR (rc)) {
                goto dtl_finalize_done;
            }
        }
    } else if ((*dtl_handle)->mode == DYAD_DTL_FLUX_RPC) {
#else
    if ((*dtl_handle)->mode == DYAD_DTL_FLUX_RPC) {
#endif
        if ((*dtl_handle)->private.flux_dtl_handle != NULL) {
            rc = dyad_dtl_flux_finalize (dtl_handle);
            if (DYAD_IS_ERROR (rc)) {
                goto dtl_finalize_done;
            }
        }
    } else {
        rc = DYAD_RC_BADDTLMODE;
        goto dtl_finalize_done;
    }
    rc = DYAD_RC_OK;

dtl_finalize_done:
    free (*dtl_handle);
    *dtl_handle = NULL;
    return rc;
}
