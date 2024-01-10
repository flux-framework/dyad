#include <caliper/ConfigManager.h>
#include <caliper/cali.h>
#include <dyad/perf/dyad_perf.h>

#include <cstring>
#include <ctime>

extern "C" {
struct dyad_perf_impl {
    void* config_manager;
};
}

void dyad_perf_cali_region_begin (dyad_perf_t* self, const char* region_name)
{
    if (self->enabled) {
        cali_begin_region (region_name);
    }
}

void dyad_perf_cali_region_end (dyad_perf_t* self, const char* region_name)
{
    if (self->enabled) {
        cali_end_region (region_name);
    }
}

void dyad_perf_cali_flush (dyad_perf_t* self)
{
    if (!self->is_module)
        return;
    cali::ConfigManager* mgr = (cali::ConfigManager*)self->priv->config_manager;
    printf ("Calling ConfigManager->flush\n");
    mgr->flush ();
    return;
}

dyad_rc_t dyad_perf_setopts (optparse_t* opts)
{
    const struct optparse_option cali_config_opt = {.name = "cali_config",
                                                    .key = 'c',
                                                    .has_arg = 1,
                                                    .arginfo = "CALI_CONFIG",
                                                    .usage =
                                                        "If provided, enable Caliper "
                                                        "annotations with the provided "
                                                        "configuration"};
    if (optparse_add_option (opts, &cali_config_opt) < 0)
        return DYAD_RC_BAD_CLI_ARG_DEF;
    return DYAD_RC_OK;
}

dyad_rc_t dyad_perf_init (dyad_perf_t** perf_handle, bool is_module, optparse_t* opts)
{
    dyad_rc_t rc = DYAD_RC_OK;
    const char* optargp = NULL;
    if (perf_handle == NULL && *perf_handle == NULL) {
        rc = DYAD_RC_PERF_INIT_FAIL;
        goto perf_init_done;
    }
    *perf_handle = (dyad_perf_t*)malloc (sizeof (struct dyad_perf));
    if (*perf_handle == NULL) {
        rc = DYAD_RC_SYSFAIL;
        // Since we couldn't allocate the handle,
        // we don't need to cleanup anything.
        // So, we go to perf_init_done.
        goto perf_init_done;
    }
    (*perf_handle)->priv = NULL;
    (*perf_handle)->is_module = is_module;
    (*perf_handle)->enabled = true;
    (*perf_handle)->region_begin = dyad_perf_cali_region_begin;
    (*perf_handle)->region_end = dyad_perf_cali_region_end;
    (*perf_handle)->flush = dyad_perf_cali_flush;
    if (is_module) {
        if (optparse_getopt (opts, "cali_config", &optargp) <= 0) {
            (*perf_handle)->enabled = false;
        } else {
            printf ("Found 'cali_config': %s\n", optargp);
            cali::ConfigManager* mgr = new cali::ConfigManager ();
            mgr->add (optargp);
            (*perf_handle)->priv = (dyad_perf_impl_t*)malloc (sizeof (struct dyad_perf_impl));
            if ((*perf_handle)->priv == NULL) {
                delete mgr;
                rc = DYAD_RC_SYSFAIL;
                goto perf_init_error;
            }
            (*perf_handle)->priv->config_manager = (void*)mgr;
            mgr->start ();
        }
    }
    rc = DYAD_RC_OK;
    goto perf_init_done;

perf_init_error:
    dyad_perf_finalize (perf_handle);

perf_init_done:
    return rc;
}

dyad_rc_t dyad_perf_finalize (dyad_perf_t** perf_handle)
{
    dyad_rc_t rc = DYAD_RC_OK;
    if (perf_handle == NULL || *perf_handle == NULL) {
        goto perf_finalize_done;
    }
    if ((*perf_handle)->is_module && (*perf_handle)->priv != NULL) {
        cali::ConfigManager* mgr = (cali::ConfigManager*)(*perf_handle)->priv->config_manager;
        (*perf_handle)->priv->config_manager = NULL;
        if (mgr != NULL)
            delete mgr;
        free ((*perf_handle)->priv);
        (*perf_handle)->priv = NULL;
    }
    free (*perf_handle);
    rc = DYAD_RC_OK;

perf_finalize_done:
    return rc;
}