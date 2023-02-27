#ifndef __FLUX_DTL_H__
#define __FLUX_DTL_H__

#include "dyad_flux_log.h"
#include "dyad_rc.h"

#include <jansson.h>

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
#endif

struct dyad_dtl_flux {
    flux_t *h;
    const char *kvs_namespace;
    flux_future_t *f;
};

typedef struct dyad_dtl_flux dyad_dtl_flux_t;

dyad_rc_t dyad_dtl_flux_init(flux_t *h, const char *kvs_namespace,
        bool debug, dyad_dtl_flux_t **dtl_handle);

dyad_rc_t dyad_dtl_flux_rpc_pack(dyad_dtl_flux_t *dtl_handle,
        const char *upath, uint32_t producer_rank, json_t **packed_obj);

dyad_rc_t dyad_dtl_flux_recv_rpc_response(dyad_dtl_flux_t *dtl_handle,
        flux_future_t *f);

dyad_rc_t dyad_dtl_flux_establish_connection(dyad_dtl_flux_t *dtl_handle);

dyad_rc_t dyad_dtl_flux_recv(dyad_dtl_flux_t *dtl_handle,
        void **buf, size_t *buflen);

dyad_rc_t dyad_dtl_flux_close_connection(dyad_dtl_flux_t *dtl_handle);

dyad_rc_t dyad_dtl_flux_finalize(dyad_dtl_flux_t *dtl_handle);

#endif /* __FLUX_DTL_H__ */
