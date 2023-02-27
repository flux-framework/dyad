#ifndef __DYAD_DTL_DEFS_H__
#define __DYAD_DTL_DEFS_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dyad_dtl dyad_dtl_t;

enum dyad_dtl_mode {
    DYAD_DTL_FLUX_RPC = 0,
    DYAD_DTL_UCX = 1,
    // TODO Figure out how to use Flux RPC
    // as a fallback if other DTLs fail
    // DYAD_DTL_UCX_W_FALLBACK,
};

typedef enum dyad_dtl_mode dyad_dtl_mode_t;

#ifdef __cplusplus
}
#endif

#endif /* __DYAD_DTL_DEFS_H__ */
