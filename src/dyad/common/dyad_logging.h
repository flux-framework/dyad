#ifndef DYAD_COMMON_DYAD_LOGGING_H
#define DYAD_COMMON_DYAD_LOGGING_H
#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/common/dyad_structures_int.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// #define DYAD_NOOP_MACRO do {} while (0)

//=============================================================================
#ifdef DYAD_LOGGER_NO_LOG
//=============================================================================
#define DYAD_LOGGER_INIT() DYAD_NOOP_MACRO
#define DYAD_LOG_STDERR(...) DYAD_NOOP_MACRO
#define DYAD_LOG_STDOUT(...) DYAD_NOOP_MACRO
#define DYAD_LOG_ERROR(dyad_ctx, ...) DYAD_NOOP_MACRO
#define DYAD_LOG_WARN(dyad_ctx, ...) DYAD_NOOP_MACRO
#define DYAD_LOG_INFO(dyad_ctx, ...) DYAD_NOOP_MACRO
#define DYAD_LOG_DEBUG(dyad_ctx, ...) DYAD_NOOP_MACRO
#define DYAD_LOG_STDOUT_REDIRECT(fpath) DYAD_NOOP_MACRO
#define DYAD_LOG_STDERR_REDIRECT(fpath) DYAD_NOOP_MACRO
//=============================================================================
#else
//=============================================================================
#define DYAD_LOG_STDERR(...) fprintf(stderr, __VA_ARGS__);
#define DYAD_LOG_STDOUT(...) fprintf(stdout, __VA_ARGS__);

#ifdef DYAD_LOGGER_FLUX // FLUX -----------------------------------------------
#define DYAD_LOGGER_INIT() ;
#define DYAD_LOG_STDOUT_REDIRECT(fpath) freopen((fpath), "a+", stdout);
#define DYAD_LOG_STDERR_REDIRECT(fpath) freopen((fpath), "a+", stderr);

#ifdef DYAD_UTIL_LOGGER
#ifdef DYAD_LOGGER_LEVEL_DEBUG
#define DYAD_LOG_DEBUG(dyad_ctx, ...) DYAD_LOG_STDERR(__VA_ARGS__)
#else
#define DYAD_LOG_DEBUG(dyad_ctx, ...) DYAD_NOOP_MACRO
#endif

#ifdef DYAD_LOGGER_LEVEL_INFO
#define DYAD_LOG_INFO(dyad_ctx, ...) DYAD_LOG_STDOUT(__VA_ARGS__)
#else
#define DYAD_LOG_INFO(dyad_ctx, ...) DYAD_NOOP_MACRO
#endif

#ifdef DYAD_LOGGER_LEVEL_WARN
#define DYAD_LOG_WARN(dyad_ctx, ...) DYAD_LOG_STDOUT(__VA_ARGS__)
#else
#define DYAD_LOG_WARN(dyad_ctx, ...) DYAD_NOOP_MACRO
#endif

#ifdef DYAD_LOGGER_LEVEL_ERROR
#define DYAD_LOG_ERROR(dyad_ctx, ...) DYAD_LOG_STDERR(__VA_ARGS__)
#else
#define DYAD_LOG_ERROR(dyad_ctx, ...) DYAD_NOOP_MACRO
#endif
#else // DYAD_UTIL_LOGGER
#include <flux/core.h>
#ifdef DYAD_LOGGER_LEVEL_DEBUG
#define DYAD_LOG_DEBUG(dyad_ctx, ...)                                          \
  flux_log((flux_t *)(((dyad_ctx_t *)dyad_ctx)->h), LOG_DEBUG, __VA_ARGS__);
#else
#define DYAD_LOG_DEBUG(dyad_ctx, ...) DYAD_NOOP_MACRO
#endif

#ifdef DYAD_LOGGER_LEVEL_INFO
#define DYAD_LOG_INFO(dyad_ctx, ...)                                           \
  flux_log((flux_t *)(((dyad_ctx_t *)dyad_ctx)->h), LOG_INFO, __VA_ARGS__);
#else
#define DYAD_LOG_INFO(dyad_ctx, ...) DYAD_NOOP_MACRO
#endif

#ifdef DYAD_LOGGER_LEVEL_WARN
#define DYAD_LOG_WARN(dyad_ctx, ...) DYAD_NOOP_MACRO
#else
#define DYAD_LOG_WARN(dyad_ctx, ...) DYAD_NOOP_MACRO
#endif

#ifdef DYAD_LOGGER_LEVEL_ERROR
#define DYAD_LOG_ERROR(dyad_ctx, ...)                                          \
  flux_log_error((flux_t *)(((dyad_ctx_t *)dyad_ctx)->h), __VA_ARGS__);
#else
#define DYAD_LOG_ERROR(dyad_ctx, ...) DYAD_NOOP_MACRO
#endif
#endif                                // DYAD_UTIL_LOGGER
#elif defined(DYAD_LOGGER_CPP_LOGGER) // CPP_LOGGER ---------------------------
#include <cpp-logger/clogger.h>

#define DYAD_LOGGER_NAME "DYAD"

#ifdef DYAD_LOGGER_LEVEL_DEBUG
#define DYAD_LOGGER_INIT()                                                     \
  cpp_logger_clog_level(CPP_LOGGER_DEBUG, DYAD_LOGGER_NAME);
#elif defined(DYAD_LOGGER_LEVEL_INFO)
#define DYAD_LOGGER_INIT()                                                     \
  cpp_logger_clog_level(CPP_LOGGER_INFO, DYAD_LOGGER_NAME);
#elif defined(DYAD_LOGGER_LEVEL_WARN)
#define DYAD_LOGGER_INIT()                                                     \
  cpp_logger_clog_level(CPP_LOGGER_WARN, DYAD_LOGGER_NAME);
#else
#define DYAD_LOGGER_INIT()                                                     \
  cpp_logger_clog_level(CPP_LOGGER_ERROR, DYAD_LOGGER_NAME);
#endif

#define DYAD_LOG_STDOUT_REDIRECT(fpath) freopen((fpath), "a+", stdout);
#define DYAD_LOG_STDERR_REDIRECT(fpath) freopen((fpath), "a+", stderr);

#ifdef DYAD_LOGGER_LEVEL_DEBUG
#define DYAD_LOG_DEBUG(dyad_ctx, ...)                                          \
  cpp_logger_clog(CPP_LOGGER_DEBUG, DYAD_LOGGER_NAME, __VA_ARGS__);
#else
#define DYAD_LOG_DEBUG(dyad_ctx, ...) DYAD_NOOP_MACRO
#endif

#ifdef DYAD_LOGGER_LEVEL_INFO
#define DYAD_LOG_INFO(dyad_ctx, ...)                                           \
  cpp_logger_clog(CPP_LOGGER_INFO, DYAD_LOGGER_NAME, __VA_ARGS__);
#else
#define DYAD_LOG_INFO(dyad_ctx, ...) DYAD_NOOP_MACRO
#endif

#ifdef DYAD_LOGGER_LEVEL_WARN
#define DYAD_LOG_WARN(dyad_ctx, ...)                                           \
  cpp_logger_clog(CPP_LOGGER_WARN, DYAD_LOGGER_NAME, __VA_ARGS__);
#else
#define DYAD_LOG_WARN(dyad_ctx, ...) DYAD_NOOP_MACRO
#endif

#ifdef DYAD_LOGGER_LEVEL_ERROR
#define DYAD_LOG_ERROR(dyad_ctx, ...)                                          \
  cpp_logger_clog(CPP_LOGGER_ERROR, DYAD_LOGGER_NAME, __VA_ARGS__);
#else
#define DYAD_LOG_ERROR(dyad_ctx, ...) DYAD_NOOP_MACRO
#endif
#endif // DYAD_LOGGER_FLUX ----------------------------------------------------
//=============================================================================
#endif // DYAD_LOGGER_NO_LOG
//=============================================================================

#ifdef __cplusplus
}
#endif

#endif /* DYAD_COMMON_DYAD_LOGGING_H */
