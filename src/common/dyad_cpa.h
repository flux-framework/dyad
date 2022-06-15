/*****************************************************************************\
 *  Copyright (c) 2018 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

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
