#ifndef DYAD_COMMON_DYAD_LOGGING_H
#define DYAD_COMMON_DYAD_LOGGING_H
#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DYAD_LOGGER_NO_LOG
#define DYAD_LOGGER_INIT() ;
#define DYAD_LOG_STDERR(...) ;
#define DYAD_LOG_STDOUT(...) ;
#define DYAD_LOG_ERROR(dyad_ctx, ...) ;
#define DYAD_LOG_WARN(dyad_ctx, ...) ;
#define DYAD_LOG_INFO(dyad_ctx, ...) ;
#define DYAD_LOG_DEBUG(dyad_ctx, ...) ;
#define DYAD_LOG_STDOUT_REDIRECT(fpath) ;
#define DYAD_LOG_STDERR_REDIRECT(fpath) ;
#else
#define DYAD_LOG_STDERR(...) fprintf(stderr, __VA_ARGS__);
#define DYAD_LOG_STDOUT(...) fprintf(stdout, __VA_ARGS__);
#ifdef DYAD_LOGGER_FLUX // FLUX
#ifndef DYAD_LOGGER_NO_LOG
#include <flux/core.h>
#endif
#define DYAD_LOGGER_INIT() ;
#define DYAD_LOG_STDOUT_REDIRECT(fpath) freopen (fpath, "a+", stdout);
#define DYAD_LOG_STDERR_REDIRECT(fpath) freopen (fpath, "a+", stderr);
#ifdef DYAD_LOGGER_LEVEL_DEBUG
#define DYAD_LOG_DEBUG(dyad_ctx, ...) flux_log (dyad_ctx->h, LOG_DEBUG, __VA_ARGS__);
#else
#define DYAD_LOG_DEBUG(dyad_ctx, ...) ;
#endif
#ifdef DYAD_LOGGER_LEVEL_INFO
#define DYAD_LOG_INFO(dyad_ctx, ...) flux_log (dyad_ctx->h, LOG_INFO, __VA_ARGS__);
#else
#define DYAD_LOG_INFO(dyad_ctx, ...) ;
#endif
#ifdef DYAD_LOGGER_LEVEL_WARN
#define DYAD_LOG_WARN(dyad_ctx, ...) ;
#else
#define DYAD_LOG_WARN(dyad_ctx, ...) ;
#endif

#ifdef DYAD_LOGGER_LEVEL_ERROR
#define DYAD_LOG_ERROR(dyad_ctx, ...) flux_log_error (dyad_ctx->h, __VA_ARGS__);
#else
#define DYAD_LOG_ERROR(dyad_ctx, ...) ;
#endif
#elif defined(DYAD_LOGGER_CPP_LOGGER) // CPP_LOGGER
#ifndef DYAD_LOGGER_NO_LOG
#include <cpp-logger/clogger.h>
#endif

#define DYAD_LOGGER_NAME "DYAD"
#ifdef DYAD_LOGGER_LEVEL_DEBUG
#define DYAD_LOGGER_INIT() cpp_logger_clog_level(CPP_LOGGER_DEBUG, DYAD_LOGGER_NAME);
#elif defined(DYAD_LOGGER_LEVEL_INFO)
#define DYAD_LOGGER_INIT() cpp_logger_clog_level(CPP_LOGGER_INFO, DYAD_LOGGER_NAME);
#elif defined(DYAD_LOGGER_LEVEL_WARN)
#define DYAD_LOGGER_INIT() cpp_logger_clog_level(CPP_LOGGER_WARN, DYAD_LOGGER_NAME);
#else
#define DYAD_LOGGER_INIT() cpp_logger_clog_level(CPP_LOGGER_ERROR, DYAD_LOGGER_NAME);
#endif

#define DYAD_LOG_STDOUT_REDIRECT(fpath) freopen (fpath, "a+", stdout);
#define DYAD_LOG_STDERR_REDIRECT(fpath) freopen (fpath, "a+", stderr);
#ifdef DYAD_LOGGER_LEVEL_DEBUG
#define DYAD_LOG_DEBUG(dyad_ctx, ...) cpp_logger_clog(CPP_LOGGER_DEBUG, DYAD_LOGGER_NAME, __VA_ARGS__);
#else
#define DYAD_LOG_DEBUG(dyad_ctx, ...) ;
#endif
#ifdef DYAD_LOGGER_LEVEL_INFO
#define DYAD_LOG_INFO(dyad_ctx, ...) cpp_logger_clog(CPP_LOGGER_INFO, DYAD_LOGGER_NAME, __VA_ARGS__);
#else
#define DYAD_LOG_INFO(dyad_ctx, ...) ;
#endif
#ifdef DYAD_LOGGER_LEVEL_WARN
#define DYAD_LOG_WARN(dyad_ctx, ...) cpp_logger_clog(CPP_LOGGER_WARN, DYAD_LOGGER_NAME, __VA_ARGS__);
#else
#define DYAD_LOG_WARN(dyad_ctx, ...) ;
#endif

#ifdef DYAD_LOGGER_LEVEL_ERROR
#define DYAD_LOG_ERROR(dyad_ctx, ...) cpp_logger_clog(CPP_LOGGER_ERROR, DYAD_LOGGER_NAME, __VA_ARGS__);
#else
#define DYAD_LOG_ERROR(dyad_ctx, ...) ;
#endif
#endif // DYAD_LOGGER_FLUX
#endif // DYAD_LOGGER_NO_LOG

#ifdef __cplusplus
}
#endif

#endif /* DYAD_COMMON_DYAD_LOGGING_H */
