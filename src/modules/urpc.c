/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if defined(__cplusplus)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <ctime>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#endif // defined(__cplusplus)

#include <flux/core.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "urpc_ctx.h"
#include "utils.h"
#include "read_all.h"

#include <jansson.h>

#if !defined(URPC_LOGGING_ON) || (URPC_LOGGING_ON == 0)
#define FLUX_LOG_INFO(...) do {} while (0)
#define FLUX_LOG_ERR(...) do {} while (0)
#else
#define FLUX_LOG_INFO(h, ...) flux_log (h, LOG_INFO, __VA_ARGS__)
#define FLUX_LOG_ERR(h, ...) flux_log_error (h, __VA_ARGS__)
#endif

static void urpc_mod_fini (void) __attribute__((destructor));


void urpc_mod_fini (void)
{
    flux_t *h = flux_open (NULL, 0);

    if (h != NULL) {
    }
}

static void freectx (void *arg)
{
    urpc_server_ctx_t *ctx = (urpc_server_ctx_t *)arg;
    flux_msg_handler_delvec (ctx->handlers);
    free (ctx);
}

static urpc_server_ctx_t *getctx (flux_t *h)
{
    urpc_server_ctx_t *ctx = (urpc_server_ctx_t *) flux_aux_get (h, "urpc");

    if (!ctx) {
        ctx = (urpc_server_ctx_t *) malloc (sizeof (*ctx));
        ctx->h = h;
        ctx->debug = false;
        ctx->handlers = NULL;
        ctx->urpc_cfg_file = NULL;
        if (flux_aux_set (h, "urpc", ctx, freectx) < 0) {
            FLUX_LOG_ERR (h, "URPC_MOD: flux_aux_set() failed!\n");
            goto error;
        }
    }

    goto done;

error:;
    return NULL;

done:
    return ctx;
}


/* execute a command */
#if URPC_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
static int urpc_exec_cmd (flux_t *h, const char *cmd, void **inbuf, ssize_t *inlen)
{
    FILE *fp = NULL;
    int fd = -1;

    FLUX_LOG_INFO (h, "URPC_MOD: the requested service command: %s", cmd);

    if ((fp = popen (cmd, "r")) == NULL) {
        FLUX_LOG_INFO (h, "URPC_MOD: failed to execute the command: %s", cmd);
        goto error;
    }

    fd = fileno (fp);
    if ((*inlen = read_all (fd, inbuf)) < 0) {
        FLUX_LOG_ERR (h, "URPC_MOD: Failed to catch result from \"%s\".\n", cmd);
        goto error;
    }

    if (pclose (fp))  {
        FLUX_LOG_ERR (h, "URPC_MOD: Failed to get result from \"%s\".\n", cmd);
        goto error;
    }
    return 0;

error:
    return -1;
}


#if URPC_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif // URPC_PERFFLOW
void urpc_respond (flux_t *h, const flux_msg_t *msg, const void *inbuf, size_t inlen)
{
    if (flux_respond_raw (h, msg, inbuf, inlen) < 0) {
        FLUX_LOG_ERR (h, "URPC_MOD: %s: flux_respond", __FUNCTION__);
    } else {
        FLUX_LOG_INFO (h, "URPC_MOD: urpc_exec_request_cb() served\n");
    }
    // TODO: check if flux_respond_raw deallocates inbuf.
    // If not, deallocate it here
}

/* request callback called when urpc.exec request is invoked */
#if URPC_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
static void urpc_exec_request_cb (flux_t *h, flux_msg_handler_t *w,
                                  const flux_msg_t *msg, void *arg)
{
    //urpc_server_ctx_t *ctx = getctx (h);
    ssize_t inlen = 0;
    void *inbuf = NULL;
//    FILE *fp = NULL;
//    int fd = -1;
    uint32_t userid = 0u;
    char *cmd = NULL;
    int saved_errno = errno;
    errno = 0;

    if (flux_msg_get_userid (msg, &userid) < 0) {
        FLUX_LOG_ERR (h, "URPC_MOD: failed to get userid.\n");
        goto error;
    }

    if (flux_request_unpack (msg, NULL, "{s:s}", "cmd", &cmd) < 0) {
        FLUX_LOG_ERR (h, "URPC_MOD: could not decode call.\n");
        goto error;
    }

    if (urpc_exec_cmd (h, cmd, &inbuf, &inlen) < 0) {
        goto error;
    }

    goto done;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0) {
        FLUX_LOG_ERR (h, "URPC_MOD: %s: flux_respond_error", __FUNCTION__);
    }
    return;

done:
    urpc_respond (h, msg, inbuf, inlen);
    errno = saved_errno;
    return;
}

/* request callback called when urpc.exec request is invoked */
#if URPC_PERFFLOW
__attribute__((annotate("@critical_path()")))
#endif
static void urpc_execj_request_cb (flux_t *h, flux_msg_handler_t *w,
                                   const flux_msg_t *msg, void *arg)
{
    //urpc_server_ctx_t *ctx = getctx (h);
    ssize_t inlen = 0;
    void *inbuf = NULL;
    uint32_t userid = 0u;
    char cmd [PATH_MAX+1] = {'\0'};
    int saved_errno = errno;
    errno = 0;

    //-----------------------------------------------------
    const char *exec = NULL;
    const char *arg1 = NULL;
    const char *filename = NULL;
    const char *content = NULL;
    int n;
    int rc = 0;
    const char *cmd_json = NULL;
    json_t *jcmd = NULL;
    json_error_t error;
    //------------------------------------------------------
    if (flux_msg_get_userid (msg, &userid) < 0) {
        FLUX_LOG_ERR (h, "URPC_MOD: failed to get userid.\n");
        goto error;
    }

    if (flux_request_decode (msg, NULL, &cmd_json) < 0) {
        FLUX_LOG_ERR (h, "URPC_MOD: could not decode call.\n");
        goto error;
    }


    if (!(jcmd = json_loads(cmd_json, 0, &error))) {
        FLUX_LOG_ERR (h, "URPC_MOD: json error on line %d: %s\n", error.line, error.text);
        goto error;
    }

    // TODO: generalize the format string.
    rc = json_unpack (jcmd, "{s:s, s:[s, {s:s, s:s}, i]}",
                      "cmd", &exec, "args", &arg1, "file", &filename, "content", &content, &n);

    if (rc) {
        FLUX_LOG_ERR (h, "URPC_MOD: could not unpack '%s'.\n", cmd_json);
        json_decref (jcmd);
        goto error;
    }

    // TODO: decode content in Base64 and write into the file 'filename'
    sprintf(cmd, "%s %s %s %d\n", exec, arg1, filename, n);
    json_decref (jcmd);

    if (urpc_exec_cmd (h, cmd, &inbuf, &inlen) < 0) {
        goto error;
    }

    goto done;

error:
    if (flux_respond_error (h, msg, errno, NULL) < 0) {
        FLUX_LOG_ERR (h, "URPC_MOD: %s: flux_respond_error", __FUNCTION__);
    }
    return;

done:
    urpc_respond (h, msg, inbuf, inlen);
    errno = saved_errno;
    return;
}


static int urpc_open (flux_t *h)
{
    urpc_server_ctx_t *ctx = getctx (h);
    int rc = -1;
    char *e = NULL;

    if ((e = getenv ("URPC_MOD_DEBUG")) && atoi (e))
        ctx->debug = true;
    rc = 0;

    return rc;
}

static const struct flux_msg_handler_spec htab[] = {
    { FLUX_MSGTYPE_REQUEST, "urpc.exec",  urpc_exec_request_cb, 0 },
    { FLUX_MSGTYPE_REQUEST, "urpc.execj",  urpc_execj_request_cb, 0 },
    FLUX_MSGHANDLER_TABLE_END
};

int mod_main (flux_t *h, int argc, char **argv)
{
    //const mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    urpc_server_ctx_t *ctx = NULL;

    if (!h) {
        fprintf (stderr, "Failed to get flux handle\n");
        goto done;
    }

    ctx = getctx (h);

    if (system (NULL) == 0) {
        FLUX_LOG_ERR (ctx->h, "URPC_MOD: shell is not available.\n");
        goto error;
    } 

    if (argc != 1) {
        FLUX_LOG_ERR (ctx->h, "URPC_MOD: Missing argument. " \
                              "Requires a config file.\n");
        fprintf  (stderr,
                  "Missing argument. Requires a config file.\n");
        goto error;
    }
    (ctx->urpc_cfg_file) = argv[0];
    //mkdir_as_needed (ctx->urpc_cfg_file, m);

    if (urpc_open (h) < 0) {
        FLUX_LOG_ERR (ctx->h, "urpc_open failed");
        goto error;
    }

    fprintf (stderr, "urpc module begins using \"%s\"\n", argv[0]);
    FLUX_LOG_INFO (ctx->h, "urpc module begins using \"%s\"\n", argv[0]);

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

MOD_NAME ("urpc");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
