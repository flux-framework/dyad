#ifndef __DYAD_MOD_FLUX_DTL_H__
#define __DYAD_MOD_FLUX_DTL_H__

#include "dyad_flux_log.h"

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdbool.h>
#include <stdlib.h>
#endif

struct dyad_mod_flux_dtl {
   flux_t *h;
   bool debug;
   const flux_msg_t *msg;
};

typedef struct dyad_mod_flux_dtl dyad_mod_flux_dtl_t;

int dyad_mod_flux_dtl_init(flux_t *h, bool debug,
        dyad_mod_flux_dtl_t **dtl_handle);

int dyad_mod_flux_dtl_rpc_unpack(dyad_mod_flux_dtl_t *dtl_handle,
        const flux_msg_t *packed_obj, char **upath);

int dyad_mod_flux_dtl_rpc_respond (dyad_mod_flux_dtl_t *dtl_handle,
        const flux_msg_t *orig_msg);

int dyad_mod_flux_dtl_establish_connection(dyad_mod_flux_dtl_t *dtl_handle);

int dyad_mod_flux_dtl_send(dyad_mod_flux_dtl_t *dtl_handle, void *buf, size_t buflen);

int dyad_mod_flux_dtl_close_connection(dyad_mod_flux_dtl_t *dtl_handle);

int dyad_mod_flux_dtl_finalize(dyad_mod_flux_dtl_t *dtl_handle);

#endif /* __DYAD_MOD_FLUX_DTL_H__ */
