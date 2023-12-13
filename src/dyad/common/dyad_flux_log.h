#ifndef DYAD_DTL_DYAD_FLUX_LOG_H
#define DYAD_DTL_DYAD_FLUX_LOG_H

// clang-format off
#include <sys/types.h>
#include <stdio.h>
// clang-format on

#include <flux/core.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(DYAD_LOGGING_ON) && (DYAD_LOGGING_ON == 1) && defined(DYAD_LOGGING_FLUX) \
    && (DYAD_LOGGING_FLUX == 1)
#define DYAD_LOG_INFO_REDIRECT(fpath) \
    do {                              \
    } while (0)
#define DYAD_LOG_ERR_REDIRECT(fpath) \
    do {                             \
    } while (0)
#define DYAD_LOG_INFO(dyad_ctx, ...) flux_log (dyad_ctx->h, LOG_INFO, __VA_ARGS__)
#define DYAD_LOG_ERR(dyad_ctx, ...) flux_log_error (dyad_ctx->h, __VA_ARGS__)
#define FLUX_LOG_INFO(flux_ctx, ...) flux_log (flux_ctx, LOG_INFO, __VA_ARGS__)
#define FLUX_LOG_ERR(flux_ctx, ...) flux_log_error (flux_ctx, __VA_ARGS__)
#elif defined(DYAD_LOGGING_ON) && (DYAD_LOGGING_ON == 1) && defined(DYAD_LOGGING_PRINTF) \
    && (DYAD_LOGGING_PRINTF == 1)
#define DYAD_LOG_INFO_REDIRECT(fpath) freopen (fpath, "a+", stdout)
#define DYAD_LOG_ERR_REDIRECT(fpath) freopen (fpath, "a+", stderr)
#define DYAD_LOG_INFO(dyad_ctx, ...) \
    fprintf (stdout, __VA_ARGS__);   \
    fflush (stdout)
#define DYAD_LOG_ERR(dyad_ctx, ...) \
    fprintf (stderr, __VA_ARGS__);  \
    fflush (stderr)
#define FLUX_LOG_INFO(flux_ctx, ...) \
    fprintf (stdout, __VA_ARGS__);   \
    fflush (stdout)
#define FLUX_LOG_ERR(flux_ctx, ...) \
    fprintf (stderr, __VA_ARGS__);  \
    fflush (stderr)
#else
#define DYAD_LOG_INFO_REDIRECT(fpath) \
    do {                              \
    } while (0)
#define DYAD_LOG_ERR_REDIRECT(fpath) \
    do {                             \
    } while (0)
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
#endif

#ifdef __cplusplus
}
#endif

#endif /* DYAD_DTL_DYAD_FLUX_LOG_H */
