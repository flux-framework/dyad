/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef USER_CPA_H
#define USER_CPA_H

#if USER_CPA // :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#if defined(__cplusplus)
#include <ctime>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#else
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#endif // defined(__cplusplus)

#if defined(__cplusplus)
extern "C" {
#endif

#define UCPA_UNDEFINED 0u
#define UCPA_CONS_BEGINS 1u
#define UCPA_PROD_BEGINS 2u
#define UCPA_CONS_ENDS 3u
#define UCPA_PROD_ENDS 4u
#define UCPA_CONS_OPENS 5u
#define UCPA_PROD_CLOSES 6u
#define UCPA_NUM_OPS 7u

#define USER_NUM_CPA_PTS   "DYAD_NUM_CPA_POINTS"

extern const char *UCPA_OP_STR[];

struct user_cpa_t {
  uint8_t tag; // operation type
  struct timespec t; // high-resolution clock value
};
typedef struct user_cpa_t u_cpa_t;

struct user_ctx_t {
    u_cpa_t *t_rec;
    unsigned int t_rec_capacity;
    unsigned int t_rec_size;
    FILE *cpa_outfile;
};
typedef struct user_ctx_t u_ctx_t;

extern u_ctx_t my_ctx;

#define U_REC_TIME(ctx, Tag) do { \
    struct timespec t_now; \
    clock_gettime (CLOCK_REALTIME, &t_now); \
    if ((ctx).t_rec_size < (ctx).t_rec_capacity) { \
        ((ctx).t_rec[(ctx).t_rec_size]).tag = Tag; \
        ((ctx).t_rec[(ctx).t_rec_size ++]).t = t_now; \
    } \
} while (0)

#define U_WRITE_TIME(ctx, Tag) do { \
    char buff[100]; \
    struct timespec t_now; \
    clock_gettime (CLOCK_REALTIME, &t_now); \
    strftime (buff, sizeof (buff), "%D %T", gmtime (&(t_now.tv_sec))); \
    fprintf ((ctx).cpa_outfile, "%s.%09ld %s\n", \
             buff, t_now.tv_nsec, UCPA_OP_STR[Tag]); \
} while (0)


#define U_PRINT_CPA_CLK(OFile, Who, CPA)  do { \
    char buff[100]; \
    strftime (buff, sizeof (buff), "%D %T", gmtime (&((CPA).t.tv_sec))); \
    fprintf (OFile, "%s.%09ld %s %s\n", \
             buff, (CPA).t.tv_nsec, Who, UCPA_OP_STR[(CPA).tag]); \
} while (0)

#if defined(__cplusplus)
};
#endif

#else // USER_CPA :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#define U_REC_TIME(ctx, Tag)
#define U_WRITE_TIME(ctx, Tag)
#define U_PRINT_CPA_CLK(Who, What, TS)
#endif // USER_CPA ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

#endif // USER_CPA_H
