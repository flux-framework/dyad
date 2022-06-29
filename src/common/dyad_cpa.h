/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef DYAD_CPA_H
#define DYAD_CPA_H

#if DYAD_CPA // :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

#if defined(__cplusplus)
#include <ctime>
#else
#include <time.h>
#endif // defined(__cplusplus)

// ----------------------------------------------------------------------------
// Operations of interest for critical path analysis
// ----------------------------------------------------------------------------

#define CPA_UNDEFINED       0u
// Consumer queries KVS to find the owner of a file
#define CPA_KVS_QRY         1u
// Consumer receives the KVS reply
#define CPA_KVS_QRY_FIN     2u
// Consumer sends fetch request to the owner (module)
#define CPA_RPC_REQ         3u
// Consumer completes the request
#define CPA_RPC_REQ_FIN     4u
// Consumer receives the whole file that it has requested into its memory buffer
#define CPA_RPC_RECV        5u
// Consumer writes the buffer into a file on a (local) file storage
#define CPA_CONS_WRITE      6u
// Producer publishes the ownership of a file
#define CPA_KVS_PUB         7u
// Publication of the ownership of a file completes
#define CPA_KVS_PUB_FIN     8u
// Module colocated with the producer starts loading the requested file
#define CPA_RPC_START       9u
// Module colocated with the producer begins transmission of the file data
#define CPA_RPC_REP        10u
// Module colocated with the producer completes the transmission
#define CPA_RPC_REP_FIN    11u
#define CPA_NUM_OPS        12u

const char *CPA_OP_STR[CPA_NUM_OPS+1] =
{
  "Undefined",
  "KVS_query",
  "KVS_query_done",
  "RPC_request",
  "RPC_request_done",
  "RPC_receive",
  "CONS_write",
  "KVS_publish",
  "KVS_publish_done",
  "RPC_start",
  "RPC_reply",
  "RPC_reply_done",
  "Noop"
};

struct dyad_cpa_t {
  uint8_t tag; // operation type
  struct timespec t; // high-resolution clock value
};

typedef struct dyad_cpa_t dyad_cpa_t;


// Record timestamps for critical path analysis
#define REC_TIME(ctx, Tag) do { \
    struct timespec t_now; \
    clock_gettime (CLOCK_REALTIME, &t_now); \
    if (ctx->t_rec_size < ctx->t_rec_capacity) { \
        (ctx->t_rec[ctx->t_rec_size]).tag = Tag; \
        (ctx->t_rec[ctx->t_rec_size ++]).t = t_now; \
    } \
} while (0)

#define WRITE_TIME(ctx, Tag) do { \
    char buff[100]; \
    struct timespec t_now; \
    clock_gettime (CLOCK_REALTIME, &t_now); \
    strftime (buff, sizeof (buff), "%D %T", gmtime (&(t_now.tv_sec))); \
    fprintf (ctx->cpa_outfile, "%s.%09ld %s\n", \
             buff, t_now.tv_nsec, CPA_OP_STR[Tag]); \
} while (0)


#define PRINT_CPA_CLK(OFile, Who, CPA)  do { \
    char buff[100]; \
    strftime (buff, sizeof (buff), "%D %T", gmtime (&((CPA).t.tv_sec))); \
    fprintf (OFile, "%s.%09ld %s %s\n", \
             buff, (CPA).t.tv_nsec, Who, CPA_OP_STR[(CPA).tag]); \
} while (0)

#else // DYAD_CPA :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#define REC_TIME(ctx, Tag)
#define WRITE_TIME(ctx, Tag)
#define PRINT_CPA_CLK(Who, What, TS)
#endif // DYAD_CPA ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

#endif // DYAD_CPA_H
