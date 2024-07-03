#ifndef DYAD_C_API_DYAD_C_H
#define DYAD_C_API_DYAD_C_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/core/dyad_ctx.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dyad_ctx;
typedef struct dyad_ctx dyad_ctx_t;

__attribute__ ((__visibility__ ("default"))) int dyad_c_init (dyad_ctx_t** ctx);

__attribute__ ((__visibility__ ("default"))) int dyad_c_open (dyad_ctx_t* ctx,
                                                              const char* path,
                                                              int oflag,
                                                              ...);

__attribute__ ((__visibility__ ("default"))) int dyad_c_close (dyad_ctx_t* ctx, int fd);

__attribute__ ((__visibility__ ("default"))) int dyad_c_finalize (dyad_ctx_t** ctx);

#ifdef __cplusplus
}
#endif

#endif /* DYAD_C_API_DYAD_C_H */