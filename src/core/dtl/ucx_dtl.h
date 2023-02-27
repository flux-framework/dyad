#ifndef __UCX_DTL_H__
#define __UCX_DTL_H__

#include "dyad_flux_log.h"
#include "dyad_rc.h"

#include <ucp/api/ucp.h>
#include <jansson.h>

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
#endif

struct dyad_dtl_ucx {
    flux_t *h;
    const char *kvs_namespace;
    ucp_context_h ucx_ctx;
    ucp_worker_h ucx_worker;
    ucp_address_t *consumer_address;
    size_t addr_len;
    ucp_tag_t comm_tag;
};

typedef struct dyad_dtl_ucx dyad_dtl_ucx_t;

dyad_rc_t dyad_dtl_ucx_init(flux_t *h, const char *kvs_namespace,
        bool debug, dyad_dtl_ucx_t **dtl_handle);

dyad_rc_t dyad_dtl_ucx_rpc_pack(dyad_dtl_ucx_t *dtl_handle,
        const char *upath, uint32_t producer_rank, json_t **packed_obj);

dyad_rc_t dyad_dtl_ucx_recv_rpc_response(dyad_dtl_ucx_t *dtl_handle,
        flux_future_t *f);

dyad_rc_t dyad_dtl_ucx_establish_connection(dyad_dtl_ucx_t *dtl_handle);

dyad_rc_t dyad_dtl_ucx_recv(dyad_dtl_ucx_t *dtl_handle,
        void **buf, size_t *buflen);

dyad_rc_t dyad_dtl_ucx_close_connection(
        dyad_dtl_ucx_t *dtl_handle);

dyad_rc_t dyad_dtl_ucx_finalize(dyad_dtl_ucx_t *dtl_handle);

#endif /* __UCX_DTL_H__ */
