#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/dtl/dyad_dtl_api.h>
#include <dyad/dtl/flux_dtl.h>

#if DYAD_ENABLE_UCX_DTL || DYAD_ENABLE_UCX_RMA
#include "ucx_dtl.h"
#endif  // DYAD_ENABLE_UCX_DTL || DYAD_ENABLE_UCX_RMA

dyad_rc_t dyad_dtl_init (dyad_ctx_t* ctx,
                         dyad_dtl_mode_t mode,
                         dyad_dtl_comm_mode_t comm_mode,
                         bool debug)
{
    DYAD_C_FUNCTION_START ();
    DYAD_LOG_DEBUG (ctx, "Initializing DTL ...");
    dyad_rc_t rc = DYAD_RC_OK;
    ctx->dtl_handle = malloc (sizeof (struct dyad_dtl));
    if (ctx->dtl_handle == NULL) {
        rc = DYAD_RC_SYSFAIL;
        DYAD_LOG_ERROR (ctx, "Cannot allocate Memory");
        goto dtl_init_done;
    }
    ctx->dtl_handle->mode = mode;
#if DYAD_ENABLE_UCX_DTL || DYAD_ENABLE_UCX_RMA
    if (mode == DYAD_DTL_UCX) {
        rc = dyad_dtl_ucx_init (ctx, mode, comm_mode, debug);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERROR (ctx, "dyad_dtl_ucx_init initialization failed rc %d", rc);
            goto dtl_init_done;
        }
    } else if (mode == DYAD_DTL_FLUX_RPC) {
#else   // DYAD_ENABLE_UCX_DTL || DYAD_ENABLE_UCX_RMA
    if (mode == DYAD_DTL_FLUX_RPC) {
#endif  // DYAD_ENABLE_UCX_DTL || DYAD_ENABLE_UCX_RMA
        rc = dyad_dtl_flux_init (ctx, mode, comm_mode, debug);
        if (DYAD_IS_ERROR (rc)) {
            DYAD_LOG_ERROR (ctx, "dyad_dtl_flux_init initialization failed rc %d", rc);
            goto dtl_init_done;
        }
    } else {
        rc = DYAD_RC_BADDTLMODE;
        DYAD_LOG_ERROR (ctx,
                        "dyad_dtl_flux_init initialization failed with incorrect mode %d",
                        mode);
        goto dtl_init_done;
    }
    rc = DYAD_RC_OK;
    DYAD_LOG_DEBUG (ctx, "Finished dyad_dtl_init successfully");
dtl_init_done:;
    DYAD_C_FUNCTION_END ();
    return rc;
}

dyad_rc_t dyad_dtl_finalize (dyad_ctx_t* ctx)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    if (ctx->dtl_handle == NULL) {
        // We should only reach this line if the user has passed
        // in an already-finalized DTL handle. In that case,
        // this function should be treated as a no-op, and we
        // should return DYAD_RC_OK to indicate no error has occured
        rc = DYAD_RC_OK;
        goto dtl_finalize_done;
    }
#if DYAD_ENABLE_UCX_DTL || DYAD_ENABLE_UCX_RMA
    if ((ctx->dtl_handle)->mode == DYAD_DTL_UCX) {
        if ((ctx->dtl_handle)->private_dtl.ucx_dtl_handle != NULL) {
            rc = dyad_dtl_ucx_finalize (ctx);
            if (DYAD_IS_ERROR (rc)) {
                goto dtl_finalize_done;
            }
        }
    } else if ((ctx->dtl_handle)->mode == DYAD_DTL_FLUX_RPC) {
#else   // DYAD_ENABLE_UCX_DTL || DYAD_ENABLE_UCX_RMA
    if ((ctx->dtl_handle)->mode == DYAD_DTL_FLUX_RPC) {
#endif  // DYAD_ENABLE_UCX_DTL || DYAD_ENABLE_UCX_RMA
        if ((ctx->dtl_handle)->private_dtl.flux_dtl_handle != NULL) {
            rc = dyad_dtl_flux_finalize (ctx);
            if (DYAD_IS_ERROR (rc)) {
                goto dtl_finalize_done;
            }
        }
    } else {
        rc = DYAD_RC_BADDTLMODE;
        goto dtl_finalize_done;
    }
    rc = DYAD_RC_OK;

dtl_finalize_done:;
    free (ctx->dtl_handle);
    ctx->dtl_handle = NULL;
    DYAD_C_FUNCTION_END ();
    return rc;
}
