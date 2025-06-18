#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/dtl/dyad_dtl_api.h>
#include <dyad/dtl/flux_dtl.h>

#if defined(DYAD_ENABLE_MARGO_DTL)
#include "margo_dtl.h"
#endif  // defined (DYAD_ENABLE_MARGO_DTL)

#if defined(DYAD_ENABLE_UCX_DTL) || defined(DYAD_ENABLE_UCX_DATA_RMA)
#include "ucx_dtl.h"
#endif  // defined (DYAD_ENABLE_UCX_DTL) || defined(DYAD_ENABLE_UCX_DATA_RMA)

dyad_rc_t dyad_dtl_init (dyad_ctx_t* ctx,
                         dyad_dtl_mode_t mode,
                         dyad_dtl_comm_mode_t comm_mode,
                         bool debug)
{
    DYAD_C_FUNCTION_START ();
    dyad_rc_t rc = DYAD_RC_OK;
    ctx->dtl_handle = malloc (sizeof (struct dyad_dtl));
    if (ctx->dtl_handle == NULL) {
        rc = DYAD_RC_SYSFAIL;
        DYAD_LOG_ERROR (ctx, "Cannot allocate Memory");
        goto dtl_init_done;
    }

    ctx->dtl_handle->mode = mode;
    // clang-format off
#if defined (DYAD_ENABLE_UCX_DTL) || defined(DYAD_ENABLE_UCX_DATA_RMA)
    if (mode == DYAD_DTL_UCX) {
        rc = dyad_dtl_ucx_init (ctx, mode, comm_mode, debug);
    } else
#endif //defined (DYAD_ENABLE_UCX_DTL) || defined(DYAD_ENABLE_UCX_DATA_RMA)
#if defined (DYAD_ENABLE_MARGO_DTL)
    if (mode == DYAD_DTL_MARGO) {
        rc = dyad_dtl_margo_init (ctx, mode, comm_mode, debug);
    } else
#endif // defined (DYAD_ENABLE_MARGO_DTL)
    if (mode == DYAD_DTL_FLUX_RPC) {
        rc = dyad_dtl_flux_init (ctx, mode, comm_mode, debug);
    } else {
        rc = DYAD_RC_BADDTLMODE;
    }
    // clang-format on

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
    // clang-format off
#if defined (DYAD_ENABLE_UCX_DTL) || defined(DYAD_ENABLE_UCX_DATA_RMA)
    if ((ctx->dtl_handle)->mode == DYAD_DTL_UCX) {
        if ((ctx->dtl_handle)->private_dtl.ucx_dtl_handle != NULL) {
            rc = dyad_dtl_ucx_finalize (ctx);
        }
    } else
#endif //defined (DYAD_ENABLE_UCX_DTL) || defined(DYAD_ENABLE_UCX_DATA_RMA)
#if defined (DYAD_ENABLE_MARGO_DTL)
    if ((ctx->dtl_handle)->mode == DYAD_DTL_MARGO) {
        rc = dyad_dtl_margo_finalize (ctx);
    } else
#endif // defined (DYAD_ENABLE_MARGO_DTL)
    if ((ctx->dtl_handle)->mode == DYAD_DTL_FLUX_RPC) {
        if ((ctx->dtl_handle)->private_dtl.flux_dtl_handle != NULL) {
            rc = dyad_dtl_flux_finalize (ctx);
        }
    } else {
        rc = DYAD_RC_BADDTLMODE;
    }
    // clang-format on
    rc = DYAD_RC_OK;

dtl_finalize_done:;
    free (ctx->dtl_handle);
    ctx->dtl_handle = NULL;
    DYAD_C_FUNCTION_END ();
    return rc;
}
