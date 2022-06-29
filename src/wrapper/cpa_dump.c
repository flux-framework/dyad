/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <libgen.h> // dirname
#include <dlfcn.h>
#include <unistd.h>
#include <flux/core.h>

int main(int argc, char** argv)
{
    flux_t *h = NULL;
    flux_future_t *f2 = NULL;
    const char *reply_data = NULL;
    char dump_name [PATH_MAX+1] = {'\0'};

    uint32_t rank = 0u; // producer rank

    if (!(h = flux_open (NULL, 0))) {
        fprintf (stderr, "Failed to get flux handle\n");
        goto error;
    }

    if (flux_get_rank (h, &rank) < 0) {
        flux_log_error (h, "flux_get_rank() failed.\n");
        goto error;
    }

    if (argc == 2) {
        sprintf (dump_name, "%s/dyad_cpa_module.%u.txt", argv[1], rank);
    } else {
        sprintf (dump_name, "dyad_cpa_module.%u.txt", rank);
    }

    if (!(f2 = flux_rpc_pack (h, "dyad.cpa_dump", rank, 0,
                              "{s:s}", "dumpfile", dump_name))) {
        flux_log_error (h, "flux_rpc_pack({dyad.cpa_dump %s})", dump_name);
        goto done;
    } else {
        if (flux_rpc_get (f2, (const char **) &reply_data) < 0) {
            flux_log_error (h, "flux_rpc_get() failed for cpa dump.\n");
            goto done;
        }
    }

    if (f2 != NULL) {
        flux_future_destroy (f2);
    }

    goto done;

error:;
    return EXIT_FAILURE;

done:;
    return EXIT_SUCCESS;
}
