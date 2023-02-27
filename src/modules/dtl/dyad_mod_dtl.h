#ifndef __DYAD_MOD_DTL_H__
#define __DYAD_MOD_DTL_H__

#include <dyad_flux_log.h>

#if defined(__cplusplus)
#include <cstdlib>
#else
#include <stdbool.h>
#include <stdlib.h>
#endif /* defined(__cplusplus) */

typedef struct dyad_mod_dtl dyad_mod_dtl_t;

enum dyad_mod_dtl_mode {
    DYAD_DTL_FLUX_RPC,
    DYAD_DTL_UCX,
    // TODO Figure out how to use Flux RPC
    // as a fallback for if UCX fails
    // DYAD_DTL_UCX_W_FALLBACK,
};

typedef enum dyad_mod_dtl_mode dyad_mod_dtl_mode_t;

int dyad_mod_dtl_init(dyad_mod_dtl_mode_t mode, flux_t *h,
        bool debug, dyad_mod_dtl_t **dtl_handle);

int dyad_mod_dtl_rpc_unpack(dyad_mod_dtl_t *dtl_handle,
        const flux_msg_t *packed_obj, char **upath);

int dyad_mod_dtl_rpc_respond(dyad_mod_dtl_t *dtl_handle,
        const flux_msg_t *orig_msg);

int dyad_mod_dtl_establish_connection(dyad_mod_dtl_t *dtl_handle);

int dyad_mod_dtl_send(dyad_mod_dtl_t *dtl_handle, void *buf, size_t buflen);

int dyad_mod_dtl_close_connection(dyad_mod_dtl_t *dtl_handle);

int dyad_mod_dtl_finalize(dyad_mod_dtl_t **dtl_handle);

#endif /* __DYAD_MOD_DTL_H__ */
