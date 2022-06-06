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

#if defined(__cplusplus)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <cstddef>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stddef.h>
#endif // defined(__cplusplus)

#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <flux/core.h>
#include "dyad.h"
#include "dyad_ctx.h"
#include "utils.h"
#include "read_all.h"

#define TIME_DIFF(Tstart, Tend ) \
    ((double) (1000000000L * ((Tend).tv_sec  - (Tstart).tv_sec) + \
                              (Tend).tv_nsec - (Tstart).tv_nsec) / 1000000000L)

#if !defined(DYAD_LOGGING_ON) || (DYAD_LOGGING_ON == 0)
#define FLUX_LOG_INFO(...) do {} while (0)
#define FLUX_LOG_ERR(...) do {} while (0)
#else
#define FLUX_LOG_INFO(h, ...) flux_log (h, LOG_INFO, __VA_ARGS__)
#define FLUX_LOG_ERR(h, ...) flux_log_error (h, __VA_ARGS__)
#endif

static void dyad_mod_fini (void) __attribute__((destructor));

#if DYAD_PROFILE
static void print_stats (const dyad_mod_ctx_t *ctx)
{
    uint32_t rank; // Flux rank

    if (ctx == NULL) {
        return;
    }

    if (flux_get_rank (ctx->h, &rank) < 0) {
        FLUX_LOG_ERR (ctx->h, "flux_get_rank() failed.\n");
        return;
    }
    printf ("DYAD Profile [%u] SYNC IO MODULE: %f sec\n",
             rank, ctx->t_sync_io_mod);
    printf ("DYAD Profile [%u] RPC MODULE: %f sec\n",
             rank, ctx->t_rpc_mod);
    FLUX_LOG_INFO (ctx->h, "DYAD Profile [%u] SYNC IO MODULE: %f sec\n", \
                            rank, ctx->t_sync_io_mod);
    FLUX_LOG_INFO (ctx->h, "DYAD Profile [%u] RPC MODULE: %f sec\n", \
                            rank, ctx->t_rpc_mod);
}
#endif // DYAD_PROFILE

#if DYAD_CPA
static int open_cpa_file(dyad_mod_ctx_t *ctx, const char *ofile_name)
{
    if (ctx->cpa_outfile == NULL) {
        FLUX_LOG_INFO (ctx->h, "DYAD_MOD_CPA: Creating log file: %s\n",
                       ofile_name);
        ctx->cpa_outfile = fopen (ofile_name, "a");
        if (ctx->cpa_outfile == NULL) {
            flux_log_error (ctx->h,
                            "DYAD_MOD_CPA: Failed to create file \'%s\'!\n",
            ofile_name);
            goto error;
        }
    }

    goto done;

error:
    return -1;

done:
    return 0;
}

static void dump_clock (dyad_mod_ctx_t *ctx)
{
    unsigned int i = 0u;

    if (ctx == NULL) {
        flux_log_error (ctx->h, "Invalid context.\n");
        goto error;
    }
    if (ctx->cpa_outfile == NULL) {
        flux_log_error (ctx->h, "CPA output file has not been opened.\n");
        goto error;
    }
    if (ctx->t_rec != NULL) {
        FLUX_LOG_INFO (ctx->h, "DYAD_MOD_CPA: dump CPA log data ...\n");
        for (i = 0u; i < ctx->t_rec_size; ++i) {
            PRINT_CPA_CLK (ctx->cpa_outfile, "MOD", ctx->t_rec[i]);
        }
/*
        free (ctx->t_rec);
        ctx->t_rec = NULL;
        ctx->t_rec_capacity = 0u;
*/
        ctx->t_rec_size = 0u;
    }

    fclose (ctx->cpa_outfile);
    ctx->cpa_outfile = NULL;

    goto done;

error:
    return;

done:
    return;
}
#endif // DYAD_CPA


void dyad_mod_fini (void)
{
    flux_t *h = flux_open (NULL, 0);

    if (h != NULL) {
      #if DYAD_PROFILE
        dyad_mod_ctx_t *ctx = (dyad_mod_ctx_t *) flux_aux_get (h, "dyad");
        print_stats (ctx);
      #endif // DYAD_PROFILE
    }
}

static void freectx (void *arg)
{
    dyad_mod_ctx_t *ctx = (dyad_mod_ctx_t *)arg;
  #if DYAD_PROFILE
    print_stats (ctx);
  #endif // DYAD_PROFILE
  #if DYAD_CPA
    FLUX_LOG_INFO (ctx->h, "DYAD_MOD_CPA: Flushing log file ...\n");
    dump_clock (ctx);
    if (ctx->t_rec != NULL) {
        free (ctx->t_rec);
        ctx->t_rec = NULL;
        ctx->t_rec_capacity = 0u;
        ctx->t_rec_size = 0u;
    }
  #endif // DYAD_CPA
    flux_msg_handler_delvec (ctx->handlers);
    free (ctx);
}

static dyad_mod_ctx_t *getctx (flux_t *h)
{
    dyad_mod_ctx_t *ctx = (dyad_mod_ctx_t *) flux_aux_get (h, "dyad");

    if (!ctx) {
        ctx = (dyad_mod_ctx_t *) malloc (sizeof (*ctx));
        ctx->h = h;
        ctx->debug = false;
        ctx->handlers = NULL;
        ctx->dyad_path = NULL;
      #if DYAD_PROFILE
        ctx->t_sync_io_mod = 0.0;
        ctx->t_rpc_mod = 0.0;
      #endif // DYAD_PROFILE
      #if DYAD_CPA
        char *e = NULL;
        ctx->t_rec_size = 0u;
        ctx->t_rec_capacity = 0u;
        if ((e = getenv (DYAD_NUM_CPA_PTS))) {
            ctx->t_rec_capacity = (unsigned) atoi (e);
        }
        if (ctx->t_rec_capacity == 0u) {
            ctx->t_rec_capacity = 1000u;
        }
        ctx->t_rec
            = (dyad_cpa_t *) malloc (ctx->t_rec_capacity * sizeof (dyad_cpa_t));
        ctx->cpa_outfile = NULL;
      #endif // DYAD_CPA
        if (flux_aux_set (h, "dyad", ctx, freectx) < 0) {
            FLUX_LOG_ERR (h, "DYAD_MOD: flux_aux_set() failed!\n");
            goto error;
        }
    }

    goto done;

error:;
    return NULL;

done:
    return ctx;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
void dyad_respond (flux_t *h, const flux_msg_t *msg, const void *inbuf, size_t inlen)
{
    if (flux_respond_raw (h, msg, inbuf, inlen) < 0) {
        FLUX_LOG_ERR (h, "DYAD_MOD: %s: flux_respond", __FUNCTION__);
    } else {
        FLUX_LOG_INFO (h, "DYAD_MOD: dyad_fetch_request_cb() served\n");
    }
}
#endif // DYAD_PERFFLOW

/* request callback called when dyad.fetch request is invoked */
#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
static void dyad_fetch_request_cb (flux_t *h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg)
{
    dyad_mod_ctx_t *ctx = getctx (h);
    ssize_t inlen = 0;
    void *inbuf = NULL;
    int fd = -1;
    uint32_t userid = 0u;
    char *upath = NULL;
    char fullpath[PATH_MAX+1] = {'\0'};
  #if DYAD_PROFILE
    struct timespec t_1, t_2;
  #endif // DYAD_PROFILE
    int saved_errno = errno;
    errno = 0;

    if (flux_msg_get_userid (msg, &userid) < 0)
        goto error;

    if (flux_request_unpack (msg, NULL, "{s:s}", "upath", &upath) < 0)
        goto error;

    FLUX_LOG_INFO (h, "DYAD_MOD: requested user_path: %s", upath);

  #if DYAD_PROFILE
    clock_gettime(CLOCK_MONOTONIC_RAW, &t_1);
  #endif // DYAD_PROFILE

    REC_TIME (ctx, CPA_RPC_START);

    strncpy (fullpath, ctx->dyad_path, PATH_MAX-1);
    concat_str (fullpath, upath, "/", PATH_MAX);

  #if DYAD_SPIN_WAIT
    if (!get_stat (fullpath, 1000U, 1000L)) {
        FLUX_LOG_ERR (h, "DYAD_MOD: Failed to access info on \"%s\".\n", \
                          fullpath);
        //goto error;
    }
  #endif // DYAD_SPIN_WAIT

    fd = open (fullpath, O_RDONLY);
    if (fd < 0) {
        FLUX_LOG_ERR (h, "DYAD_MOD: Failed to open file \"%s\".\n", fullpath);
        goto error;
    }
    if ((inlen = read_all (fd, &inbuf)) < 0) {
        FLUX_LOG_ERR (h, "DYAD_MOD: Failed to load file \"%s\".\n", fullpath);
        goto error;
    }
    close (fd);

  #if DYAD_PROFILE
    clock_gettime(CLOCK_MONOTONIC_RAW, &t_2);
    ctx->t_sync_io_mod += TIME_DIFF(t_1, t_2);
  #endif // DYAD_PROFILE

    goto done;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0) {
        FLUX_LOG_ERR (h, "DYAD_MOD: %s: flux_respond_error", __FUNCTION__);
    }
    return;

done:
#if DYAD_PERFFLOW
    dyad_respond (h, msg, inbuf, inlen);
#else
  #if DYAD_PROFILE
    clock_gettime(CLOCK_MONOTONIC_RAW, &t_1);
  #endif // DYAD_PROFILE
    REC_TIME (ctx, CPA_RPC_REP);
    //if (flux_respond_raw (h, msg, errno, inbuf, inlen) < 0)
    if (flux_respond_raw (h, msg, inbuf, inlen) < 0) {
        FLUX_LOG_ERR (h, "DYAD_MOD: %s: flux_respond", __FUNCTION__);
    } else {
        FLUX_LOG_INFO (h, "DYAD_MOD: dyad_fetch_request_cb() served %s\n", \
                           fullpath);
    }
    // TODO: check if flux_respond_raw deallocates inbuf.
    // If not, deallocate it here
  #if DYAD_PROFILE
    clock_gettime(CLOCK_MONOTONIC_RAW, &t_2);
    ctx->t_rpc_mod += TIME_DIFF(t_1, t_2);
  #endif // DYAD_PROFILE
    REC_TIME (ctx, CPA_RPC_REP_FIN);
#endif // DYAD_PERFFLOW
    errno = saved_errno;
    return;
}

#if DYAD_CPA
/* request callback called when dyad.fetch request is invoked */
static void dyad_cpa_dump_request_cb (flux_t *h, flux_msg_handler_t *w,
                                      const flux_msg_t *msg, void *arg)
{
    dyad_mod_ctx_t *ctx = getctx (h);
    uint32_t userid = 0u;
    char *dump_file = NULL;
    char fullpath[PATH_MAX+1] = {'\0'};
    int saved_errno = errno;
    errno = 0;

    if (flux_msg_get_userid (msg, &userid) < 0)
        goto error;

    if (flux_request_unpack (msg, NULL, "{s:s}", "dumpfile", &dump_file) < 0)
        goto error;

    FLUX_LOG_INFO (h, "DYAD_MOD: CPA dump path: %s", dump_file);

    strncpy (fullpath, dump_file, PATH_MAX-1);

    errno = saved_errno;
    goto done;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0) {
        FLUX_LOG_ERR (h, "DYAD_MOD: %s: flux_respond_error", __FUNCTION__);
    }
    return;

done:
    if (flux_respond (h, msg, NULL) < 0) {
        FLUX_LOG_ERR (h, "DYAD_MOD: %s: flux_respond", __FUNCTION__);
    } else {
        FLUX_LOG_INFO (h, "DYAD_MOD: dyad_cpa_dump_request_cb() served %s\n", \
                           fullpath);
    }
    if (open_cpa_file (ctx, fullpath) < 0) {
        goto error;
    }
    dump_clock (ctx);

    errno = saved_errno;
    return;
}
#endif // DYAD_CPA

static int dyad_open (flux_t *h)
{
    dyad_mod_ctx_t *ctx = getctx (h);
    int rc = -1;
    char *e = NULL;

    if ((e = getenv ("DYAD_MOD_DEBUG")) && atoi (e))
        ctx->debug = true;
    rc = 0;

    return rc;
}

static const struct flux_msg_handler_spec htab[] = {
    { FLUX_MSGTYPE_REQUEST, "dyad.fetch",  dyad_fetch_request_cb, 0 },
  #if DYAD_CPA
    { FLUX_MSGTYPE_REQUEST, "dyad.cpa_dump",  dyad_cpa_dump_request_cb, 0 },
  #endif // DYAD_CPA
    FLUX_MSGHANDLER_TABLE_END
};

int mod_main (flux_t *h, int argc, char **argv)
{
    const mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    dyad_mod_ctx_t *ctx = NULL;

    if (!h) {
        fprintf (stderr, "Failed to get flux handle\n");
        goto done;
    }

    ctx = getctx (h);

    if (argc != 1) {
        FLUX_LOG_ERR (ctx->h, "DYAD_MOD: Missing argument. " \
                              "Requires a local dyad path specified.\n");
        fprintf  (stderr,
                  "Missing argument. Requires a local dyad path specified.\n");
        goto error;
    }
    (ctx->dyad_path) = argv[0];
    mkdir_as_needed (ctx->dyad_path, m);

    if (dyad_open (h) < 0) {
        FLUX_LOG_ERR (ctx->h, "dyad_open failed");
        goto error;
    }

    fprintf (stderr, "dyad module begins using \"%s\"\n", argv[0]);
    FLUX_LOG_INFO (ctx->h, "dyad module begins using \"%s\"\n", argv[0]);

    if (flux_msg_handler_addvec (ctx->h, htab, (void *)h,
                                 &ctx->handlers) < 0) {
        FLUX_LOG_ERR (ctx->h, "flux_msg_handler_addvec: %s\n", strerror (errno));
        goto error;
    }

    if (flux_reactor_run (flux_get_reactor (ctx->h), 0) < 0) {
        FLUX_LOG_ERR (ctx->h, "flux_reactor_run: %s", strerror (errno));
        goto error;
    }

    goto done;

error:;
    return EXIT_FAILURE;

done:;
    return EXIT_SUCCESS;
}

MOD_NAME ("dyad");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */