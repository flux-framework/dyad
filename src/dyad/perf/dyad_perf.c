#if !defined(WITH_CALIPER)
#include <dyad/perf/dyad_perf.h>

#include <stdlib.h>

void dyad_perf_default_region_begin (dyad_perf_t* restrict self, const char* restrict region_name)
{
    return;
}

void dyad_perf_default_region_end (dyad_perf_t* restrict self, const char* restrict region_name)
{
    return;
}

void dyad_perf_default_flush (dyad_perf_t* restrict self)
{
    return;
}

dyad_rc_t dyad_perf_setopts (optparse_t* opts)
{
    return DYAD_RC_OK;
}

dyad_rc_t dyad_perf_init (dyad_perf_t** perf_handle, bool is_module, optparse_t* opts)
{
    dyad_rc_t rc = DYAD_RC_OK;
    if (perf_handle == NULL && *perf_handle == NULL) {
        rc = DYAD_RC_PERF_INIT_FAIL;
        goto perf_init_done;
    }
    *perf_handle = (dyad_perf_t*)malloc (sizeof (struct dyad_perf));
    if (*perf_handle == NULL) {
        rc = DYAD_RC_SYSFAIL;
        goto perf_init_done;
    }
    (*perf_handle)->priv = NULL;
    (*perf_handle)->is_module = is_module;
    (*perf_handle)->enabled = false;
    (*perf_handle)->region_begin = dyad_perf_default_region_begin;
    (*perf_handle)->region_end = dyad_perf_default_region_end;
    (*perf_handle)->flush = dyad_perf_default_flush;

perf_init_done:
    return rc;
}

dyad_rc_t dyad_perf_finalize (dyad_perf_t** perf_handle)
{
    if (perf_handle == NULL || *perf_handle == NULL) {
        free (*perf_handle);
    }
    return DYAD_RC_OK;
}

#endif /* defined(WITH_CALIPER) */