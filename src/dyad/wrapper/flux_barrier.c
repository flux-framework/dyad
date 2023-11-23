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
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <flux/core.h>
#if DYAD_FULL_DEBUG
#define IPRINTF DPRINTF
#define IPRINTF_DEFINED
#else
#define IPRINTF(fmt, ...)
#endif  // DYAD_FULL_DEBUG

#if !defined(DYAD_LOGGING_ON) || (DYAD_LOGGING_ON == 0)
#define FLUX_LOG_INFO(...) \
    do {                   \
    } while (0)
#define FLUX_LOG_ERR(...) \
    do {                  \
    } while (0)
#else
#define FLUX_LOG_INFO(...) flux_log (h, LOG_INFO, __VA_ARGS__)
#define FLUX_LOG_ERR(...) flux_log_error (h, __VA_ARGS__)
#endif

int main (int argc, char** argv)
{
    flux_future_t* fb;
    flux_t* h;
    uint32_t n_ranks = 1u;
    uint32_t my_rank = 0u;
    const char* tag = "flux_barrier";

    if (!(h = flux_open (NULL, 0))) {
        fprintf (stderr, "Can't open flux\n");
    }

    if (argc != 2) {
        FLUX_LOG_ERR (
            "flux_barrier requires a tag to distinguish different "
            "synchronizations\n");
        return EXIT_FAILURE;
    }
    tag = argv[1];

#if 1
    if (flux_get_size (h, &n_ranks) < 0) {
        FLUX_LOG_ERR ("flux_get_size() failed");
    }
    FLUX_LOG_INFO ("FLUX Instance Size: %u\n", n_ranks);
#else
    if (argc != 3) {
        FLUX_LOG_ERR ("flux_barrier requires number of ranks to synchronize\n");
        return EXIT_FAILURE;
    }

    n_ranks = (uint32_t)atoi (argv[2]);
#endif
    my_rank = 0u;

    if ((flux_get_rank (h, &my_rank) < 0) || (my_rank >= n_ranks)) {
        FLUX_LOG_ERR (
            "flux_get_rank() failed or "
            "an invalid number of ranks is given.\n");
        return EXIT_FAILURE;
    }

    FLUX_LOG_INFO ("Before barrier '%s' with rank %u\n", tag, my_rank);

    if (!(fb = flux_barrier (h, tag, n_ranks))) {
        FLUX_LOG_ERR ("flux_barrier failed for %d ranks\n", n_ranks);
        flux_future_destroy (fb);
        return EXIT_FAILURE;
    }

    if (flux_future_get (fb, NULL) < 0) {
        FLUX_LOG_ERR ("flux_future_get for barrir failed\n");
        flux_future_destroy (fb);
        return EXIT_FAILURE;
    }

    flux_future_destroy (fb);

//    struct timespec t_now;
//    clock_gettime (CLOCK_REALTIME, &t_now);
//    char tbuf[100];
//    strftime (tbuf, sizeof (tbuf), "%D %T", gmtime (&(t_now.tv_sec)));
//
//    FLUX_LOG_INFO ("Synchronized at %s.%09ld\n", tbuf, t_now.tv_nsec);

    return EXIT_SUCCESS;
}
