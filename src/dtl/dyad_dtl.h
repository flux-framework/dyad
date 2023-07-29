#ifndef DYAD_DTL_H
#define DYAD_DTL_H

#ifdef __cplusplus
extern "C" {
#endif
    
enum dyad_dtl_mode {
    DYAD_DTL_UCX = 0,
    DYAD_DTL_FLUX_RPC = 1,
    DYAD_DTL_END = 2
};
typedef enum dyad_dtl_mode dyad_dtl_mode_t;

struct dyad_dtl;

#ifdef __cplusplus
}
#endif

#endif /* DYAD_DTL_H */