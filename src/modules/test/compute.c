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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <flux/core.h>
#include <linux/limits.h>
#include <flux/core.h>
#include <stdio.h>
#include <flux/core.h>
#include <libgen.h> // dirname
#include "utils.h"
#include "dyad.h"

static int publish_via_flux (FILE* df, flux_t* h, const char *user_path)
{
    int rc = -1;
    uint32_t owner_rank = 0;
    flux_kvs_txn_t *txn = NULL;
    flux_future_t *f = NULL;

    const size_t topic_len = PATH_MAX;
    char topic [topic_len + 1];

    memset (topic, '\0', topic_len + 1);
    strncpy (topic, user_path, topic_len);

    if (!h)
        goto done;

    fprintf (df, "Assume the existence of key \"%s\"\n", topic);

    if (flux_get_rank (h, &owner_rank) < 0) {
        fprintf (df, "flux_get_rank() failed");
        goto done;
    }

    // Register the owner of the file into key-value-store
    if (!(txn = flux_kvs_txn_create ())) {
        fprintf (df, "flux_kvs_txn_create() failed.\n");
        goto done;
    }
    if (flux_kvs_txn_pack (txn, 0, topic, "i", owner_rank) < 0) {
        fprintf (df, "flux_kvs_txn_pack(\"%s\",\"i\",%u) failed.\n",
                 topic, owner_rank);
        goto done;
    }

    if (!(f = flux_kvs_commit (h, getenv ("FLUX_KVS_NAMESPACE"), 0, txn))) {
        fprintf (df, "flux_kvs_commit(owner rank of %s = %u)\n",
                 user_path, owner_rank);
        goto done;
    }

    //flux_future_get (f, NULL);
    flux_future_wait_for (f, -1.0);
    flux_future_destroy (f);
    f = NULL;
    flux_kvs_txn_destroy (txn);

    rc = 0;

done:
    return rc;
}


int main (int argc, char **argv)
{
    if (argc != 3) {
        printf("usage> %s file_to_fetch out_dir\n", argv[0]);
        return (0);
    }

    flux_t *h = NULL;
    flux_future_t *f1 = NULL, *f2 = NULL;
    const char* file_name = argv[1];
    const char* out_dir = argv[2];
    FILE *df = fopen("debug_consumer.txt", "w");

    if (!df) {
        return (0);
    }
    fprintf(df, "Consumer begins with %s and %s\n", file_name, out_dir);

    if (!(h = flux_open (NULL, 0))) {
        fclose(df);
        return (0);
    }
    fprintf(df, "Consumer connected to flux.\n");

    publish_via_flux(df, h, file_name);

    // Look up key-value-store for the owner of a file
    uint32_t owner_rank = 0;
    const size_t topic_len = PATH_MAX;
    char topic [topic_len + 1];

    memset (topic, '\0', topic_len + 1);
    strncpy (topic, file_name, topic_len);


    fprintf(df, "Consumer kvs search for %s\n", file_name);
    f1 = flux_kvs_lookup (h, getenv ("FLUX_KVS_NAMESPACE"),
                          FLUX_KVS_WAITCREATE, topic);
    if (f1 == NULL) {
        flux_log_error (h, "flux_kvs_lookup(%s) failed.\n", topic);
        goto done;
    }

    // Extract the owner rank from the reply
    if (flux_kvs_lookup_get_unpack (f1, "i", &owner_rank) < 0) {
        flux_log_error (h, "flux_kvs_lookup_get_unpack() failed.\n");
        goto done;
    }
    flux_future_destroy (f1);
    f1 = NULL;

    fprintf(df, "Owner rank: %u\n", owner_rank);

    // Send the request to fetch a file
    if (!(f2 = flux_rpc_pack (h, "dyad.fetch", owner_rank, 0,
		              "{s:s}", "upath", file_name))) {
        flux_log_error (h, "flux_rpc_pack({dyad.fetch %s})", file_name);
        goto done;
    }
    fprintf(df, "Consumer flux_rpc_pack() done\n");

    // Extract the data from reply
    const char* file_data = NULL;
    int file_len = 0;
    if (flux_rpc_get_raw (f2, (const void **) &file_data, &file_len) < 0) {
        flux_log_error (h, "flux_rpc_get_raw(\"%s\") failed.\n",
                        file_name);
        goto done;
    }
    fprintf (df, "Consumer file size: %d\n", file_len);

    // Set output file path
    char file_path[PATH_MAX+1] = {'\0'};
    char file_path_copy[PATH_MAX+1] = {'\0'};

    strncpy (file_path, out_dir, PATH_MAX-1);
    concat_str (file_path, file_name, "/", PATH_MAX);
    strncpy (file_path_copy, file_path, PATH_MAX-1);

    // Create the directory as needed
    const char *odir = dirname (file_path_copy);
    const mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    if ((strncmp (odir, ".", strlen(".")) != 0) &&
        (mkdir_as_needed (odir, m) < 0)) {
        fprintf (stderr, "Failed to create directory \"%s\".\n", odir);
        goto done;
    }

    // Write the file
    FILE *of = fopen (file_path, "w");
    if (of == NULL) {
        fprintf (df, "Could not write file: %s\n", file_path);
        goto done;
    }
    fwrite (file_data, sizeof(char), (size_t) file_len, of);
    fclose (of);

    flux_future_destroy (f2);
    f2 = NULL;

done:

    fclose(df);
    if (f1 != NULL) {
        flux_future_destroy (f1);
    }
    if (f2 != NULL) {
        flux_future_destroy (f2);
    }
    flux_close (h);
    return (0);
}
