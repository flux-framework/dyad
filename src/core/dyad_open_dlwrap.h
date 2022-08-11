#ifndef __DYAD_CORE_DYAD_OPEN_DLWRAP_H__
#define __DYAD_CORE_DYAD_OPEN_DLWRAP_H__

#include "dyad_core.h"

FILE *dyad_fopen(dyad_ctx_t *ctx, const char *path, const char *mode);

int dyad_open(dyad_ctx_t *ctx, const char *path, int oflag, ...);

int dyad_fclose(dyad_ctx_t *ctx, FILE *fp);

int dyad_close(dyad_ctx_t *ctx, int fd);

#endif /* __DYAD_CORE_DYAD_OPEN_DLWRAP_H__ */
