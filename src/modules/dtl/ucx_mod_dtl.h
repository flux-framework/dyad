#ifndef __DYAD_MOD_UCX_DTL_H__
#define __DYAD_MOD_UCX_DTL_H__

#include "dyad_flux_log.h"
#include <ucp/api/ucp.h>

#if defined(__cplusplus)
#include <cstdlib>
#else
#include <stdbool.h>
#include <stdlib.h>
#endif

struct dyad_mod_ucx_dtl {
    flux_t *h;
    bool debug;
    ucp_context_h ucx_ctx;
    ucp_worker_h ucx_worker;
    ucp_ep_h curr_ep;
    ucp_address_t *curr_cons_addr;
    size_t addr_len;
    ucp_tag_t curr_comm_tag;
};

typedef struct dyad_mod_ucx_dtl dyad_mod_ucx_dtl_t;

int dyad_mod_ucx_dtl_init(flux_t *h, bool debug, dyad_mod_ucx_dtl_t **dtl_handle);

int dyad_mod_ucx_dtl_rpc_unpack(dyad_mod_ucx_dtl_t *dtl_handle,
        const flux_msg_t *packed_obj, char **upath);

int dyad_mod_ucx_dtl_rpc_respond (dyad_mod_ucx_dtl_t *dtl_handle,
        const flux_msg_t *orig_msg);

int dyad_mod_ucx_dtl_establish_connection(dyad_mod_ucx_dtl_t *dtl_handle);

int dyad_mod_ucx_dtl_send(dyad_mod_ucx_dtl_t *dtl_handle, void *buf, size_t buflen);

int dyad_mod_ucx_dtl_close_connection(dyad_mod_ucx_dtl_t *dtl_handle);

int dyad_mod_ucx_dtl_finalize(dyad_mod_ucx_dtl_t *dtl_handle);

#endif /* __DYAD_MOD_UCX_DTL_H__ */
