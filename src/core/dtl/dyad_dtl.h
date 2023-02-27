#ifndef __DYAD_DTL_H__
#define __DYAD_DTL_H__

#include "dyad_dtl_defs.h"
#include "dyad_rc.h"

#include <flux/core.h>
#include <jansson.h>

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdbool.h>
#include <stdint.h>
#endif

dyad_rc_t dyad_dtl_init(dyad_dtl_mode_t mode, flux_t *h,
        const char *kvs_namespace, bool debug,
        dyad_dtl_t **dtl_handle);

dyad_rc_t dyad_dtl_rpc_pack(dyad_dtl_t *dtl_handle,
        const char *upath, uint32_t producer_rank, json_t **packed_obj);

dyad_rc_t dyad_dtl_recv_rpc_response(dyad_dtl_t *dtl_handle,
        flux_future_t *f);

dyad_rc_t dyad_dtl_establish_connection(dyad_dtl_t *dtl_handle);

dyad_rc_t dyad_dtl_recv(dyad_dtl_t *dtl_handle,
        void **buf, size_t *buflen);

dyad_rc_t dyad_dtl_close_connection(dyad_dtl_t *dtl_handle);

dyad_rc_t dyad_dtl_finalize(dyad_dtl_t **dtl_handle);

#endif /* __DYAD_DTL_H__ */
