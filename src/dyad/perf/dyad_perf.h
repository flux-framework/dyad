#ifndef DYAD_PERF_DYAD_PERF_H
#define DYAD_PERF_DYAD_PERF_H

#include <dyad/common/dyad_rc.h>

#include "flux/optparse.h"

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

struct dyad_perf_impl;

typedef struct dyad_perf_impl dyad_perf_impl_t;

struct dyad_perf {
    bool is_module;
    bool enabled;
    dyad_perf_impl_t* priv;
    void (*region_begin) (struct dyad_perf* self, const char* region_name);
    void (*region_end) (struct dyad_perf* self, const char* region_name);
    void (*flush) (struct dyad_perf* self);
};

typedef struct dyad_perf dyad_perf_t;

DYAD_DLL_HIDDEN dyad_rc_t dyad_perf_setopts (optparse_t* opts);

DYAD_DLL_HIDDEN dyad_rc_t dyad_perf_init (dyad_perf_t** perf_handle,
                                          bool is_module,
                                          optparse_t* opts);

DYAD_DLL_HIDDEN dyad_rc_t dyad_perf_finalize (dyad_perf_t** perf_handle);

#define DYAD_PERF_REGION_BEGIN(handle, name) \
    if (handle != NULL) { \
        handle->region_begin (handle, name); \
    }
#define DYAD_PERF_REGION_END(handle, name) \
    if (handle != NULL) { \
        handle->region_end (handle, name); \
    }

#ifdef __cplusplus
}
#endif

#endif /* DYAD_PERF_DYAD_PERF_H */