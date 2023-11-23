#ifndef DYAD_DTL_DYAD_FLUX_LOG_H
#define DYAD_DTL_DYAD_FLUX_LOG_H

#include <sys/types.h>
#include <unistd.h>
#include <flux/core.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(DYAD_LOGGING_ON) || (DYAD_LOGGING_ON == 0)
#define DYAD_LOG_INFO(dyad_ctx, ...) \
    do {                             \
    } while (0)
#define DYAD_LOG_ERR(dyad_ctx, ...) \
    do {                            \
    } while (0)
#define FLUX_LOG_INFO(flux_ctx, ...) \
    do {                             \
    } while (0)
#define FLUX_LOG_ERR(flux_ctx, ...) \
    do {                            \
    } while (0)
#else
#define DYAD_LOG_INFO(dyad_ctx, ...) flux_log (dyad_ctx->h, LOG_INFO, __VA_ARGS__)
#define DYAD_LOG_ERR(dyad_ctx, ...) flux_log_error (dyad_ctx->h, __VA_ARGS__)
#define FLUX_LOG_INFO(flux_ctx, ...) flux_log (flux_ctx, LOG_INFO, __VA_ARGS__)
#define FLUX_LOG_ERR(flux_ctx, ...) flux_log_error (flux_ctx, __VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif /* DYAD_DTL_DYAD_FLUX_LOG_H */
