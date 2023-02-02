/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <cerrno>
#include <cstdarg>
#include <ctime>
using namespace std; // std::clock ()
//#include <cstdbool> // c++11

#include <fcntl.h>
#include <libgen.h> // dirname
#include <dlfcn.h>
#include <unistd.h>
#include <flux/core.h>
#include "dyad.h"
#include "dyad_stream_core.hpp"
#include "utils.h"
#include "murmur3.h"

// Debug message
#ifndef DPRINTF
#define DPRINTF(fmt,...) do { \
    if (m_debug) fprintf (stderr, fmt, ##__VA_ARGS__); \
} while (0)
#endif // DPRINTF

#define TIME_DIFF(Tstart, Tend ) \
    ((double) (1000000000L * ((Tend).tv_sec  - (Tstart).tv_sec) + \
                              (Tend).tv_nsec - (Tstart).tv_nsec) / 1000000000L)

// Detailed information message that can be omitted
#if DYAD_FULL_DEBUG
//#define IPRINTF DPRINTF
#define IPRINTF FLUX_LOG_INFO
#define IPRINTF_DEFINED
#else
#define IPRINTF(fmt,...)
#endif // DYAD_FULL_DEBUG

#if !defined(DYAD_LOGGING_ON) || (DYAD_LOGGING_ON == 0)
#define FLUX_LOG_INFO(...) do {} while (0)
#define FLUX_LOG_ERR(...) do {} while (0)
#else
#define FLUX_LOG_INFO(...) flux_log (m_flux, LOG_INFO, __VA_ARGS__)
#define FLUX_LOG_ERR(...) flux_log_error (m_flux, __VA_ARGS__)
#endif


namespace dyad {

#if DYAD_SYNC_DIR
int sync_directory (const char* path);
#endif // DYAD_SYNC_DIR

/*****************************************************************************
 *                                                                           *
 *                   DYAD Sync Internal API                                  *
 *                                                                           *
 *****************************************************************************/

static int gen_path_key (const char* str, char* path_key, const size_t len,
    const uint32_t depth, const uint32_t width)
{
    static const uint32_t seeds [10]
        = {104677, 104681, 104683, 104693, 104701,
           104707, 104711, 104717, 104723, 104729};

    uint32_t seed = 57;
    uint32_t hash[4] = {0u}; // Output for the hash
    size_t cx = 0ul;
    int n = 0;

    if (path_key == NULL || len == 0ul) {
        return -1;
    }
    path_key [0] = '\0';

    for (uint32_t d = 0u; d < depth; d++)
    {
        seed += seeds [d % 10];
        MurmurHash3_x64_128 (str, strlen (str), seed, hash);
        uint32_t bin = (hash[0] ^ hash[1] ^ hash[2] ^ hash[3]) % width;
        n = snprintf (path_key+cx, len-cx, "%x.", bin);
        cx += n;
        if (cx >= len || n < 0) {
            return -1;
        }
    }
    n = snprintf (path_key+cx, len-cx, "%s", str);
    if (cx + n >= len || n < 0) {
        return -1;
    }
    return 0;
}

/*****************************************************************************
 *                                                                           *
 *                           dyad_stream_core API                             *
 *                                                                           *
 *****************************************************************************/

void dyad_stream_core::init ()
{
    char *e = NULL;
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    DPRINTF ("DYAD_WRAPPER: Initializeing DYAD wrapper\n");

    if ((e = getenv ("DYAD_SYNC_DEBUG")) && (atoi (e) != 0)) {
        m_debug = true;
        enable_debug_dyad_utils ();
    } else {
        m_debug = false;
        disable_debug_dyad_utils ();
    }

    if ((e = getenv ("DYAD_SHARED_STORAGE")) && (atoi (e) != 0))
        m_shared_storage = true;
    else
        m_shared_storage = false;

    if ((e = getenv ("DYAD_KEY_DEPTH")))
        m_key_depth = atoi (e);
    else
        m_key_depth = 2;

    if ((e = getenv ("DYAD_KEY_BINS")))
        m_key_bins = atoi (e);
    else
        m_key_bins = 256;

    const char* ns = getenv ("DYAD_KVS_NAMESPACE");
    m_kvs_namespace = std::string ((ns == nullptr)? "" : ns);

  #if defined(TEST_WITHOUT_FLUX)
    m_flux = nullptr;
    m_rank = 0u;
  #else
    if (!(m_flux = flux_open (NULL, 0))) {
        DPRINTF ("DYAD_SYNC: can't open flux/\n");
    }

    if (flux_get_rank (m_flux, &m_rank) < 0) {
        FLUX_LOG_ERR ("flux_get_rank() failed.\n");
    }
  #endif // !defined(TEST_WITHOUT_FLUX)

    const char* path_cons = getenv (DYAD_PATH_CONS_ENV);
    const char* path_prod = getenv (DYAD_PATH_PROD_ENV);
    m_dyad_path_cons = std::string ((path_cons == nullptr)? "" : path_cons);
    m_dyad_path_prod = std::string ((path_prod == nullptr)? "" : path_prod);

    log_info ("Stream core is initialized by env variables.");
}

void dyad_stream_core::init (const dyad_params& p)
{
    m_initialized = true;

    DPRINTF ("DYAD_WRAPPER: Initializeing DYAD wrapper\n");
    m_dyad_path_cons = p.m_dyad_path_cons;
    m_dyad_path_prod = p.m_dyad_path_prod;
    m_kvs_namespace = p.m_kvs_namespace;

    m_key_depth = p.m_key_depth;
    m_key_bins = p.m_key_bins;

    m_shared_storage = p.m_shared_storage;

    m_debug = p.m_debug;
    if (m_debug) {
        enable_debug_dyad_utils ();
    } else {
        disable_debug_dyad_utils ();
    }

  #if defined(TEST_WITHOUT_FLUX)
    m_flux = nullptr;
    m_rank = 0u;
  #else
    if (!(m_flux = flux_open (NULL, 0))) {
        DPRINTF ("DYAD_SYNC: can't open flux/\n");
    }

    if (flux_get_rank (m_flux, &m_rank) < 0) {
        FLUX_LOG_ERR ("flux_get_rank() failed.\n");
    }
  #endif // !defined(TEST_WITHOUT_FLUX)

    log_info ("Stream core is initialized by parameters");
}

void dyad_stream_core::log_info (const std::string& msg_head) const
{
    FLUX_LOG_INFO ("=== %s ===\n", msg_head.c_str ());
    FLUX_LOG_INFO ("%s=%s\n", DYAD_PATH_CONS_ENV, m_dyad_path_cons.c_str ());
    FLUX_LOG_INFO ("%s=%s\n", DYAD_PATH_PROD_ENV, m_dyad_path_prod.c_str ());
    FLUX_LOG_INFO ("DYAD_SYNC_DEBUG=%s\n", (m_debug)? "true": "false");
    FLUX_LOG_INFO ("DYAD_SHARED_STORAGE=%s\n", (m_shared_storage)? "true": "false");
    FLUX_LOG_INFO ("DYAD_KEY_DEPTH=%u\n", m_key_depth);
    FLUX_LOG_INFO ("DYAD_KEY_BINS=%u\n", m_key_bins);
    FLUX_LOG_INFO ("DYAD_KVS_NAMESPACE=%s\n", m_kvs_namespace.c_str ());
}

bool dyad_stream_core::is_dyad_producer ()
{
    return !m_dyad_path_prod.empty();
}

bool dyad_stream_core::is_dyad_consumer ()
{
    return !m_dyad_path_cons.empty();
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
int dyad_stream_core::dyad_kvs_loopup (const char *topic,
                                       uint32_t *owner_rank)
{
    flux_future_t *f1 = NULL;

    // Look up key-value-store for the owner of a file
    f1 = flux_kvs_lookup (m_flux, m_kvs_namespace.c_str(),
                          FLUX_KVS_WAITCREATE, topic);
    if (f1 == NULL) {
        FLUX_LOG_ERR ("flux_kvs_lookup(%s) failed.\n", topic);
        return -1;
    }

    // Extract the value from the reply
    if (flux_kvs_lookup_get_unpack (f1, "i", owner_rank) < 0) {
        FLUX_LOG_ERR ("flux_kvs_lookup_get_unpack() failed.\n");
        return -1;
    }
    flux_future_destroy (f1);
    f1 = NULL;
    return 0;
}

__attribute__((annotate("@critical_path()")))
int dyad_stream_core::dyad_rpc_pack (flux_future_t **f2,
                                     uint32_t owner_rank,
                                     const char *user_path)
{
    // Send the request to fetch a file
    if (!(*f2 = flux_rpc_pack (m_flux, "dyad.fetch", owner_rank, 0,
                              "{s:s}", "upath", user_path))) {
        FLUX_LOG_ERR ("flux_rpc_pack({dyad.fetch %s})", user_path);
        return -1;
    }

    return 0;
}

__attribute__((annotate("@critical_path()")))
int dyad_stream_core::dyad_rpc_get_raw (flux_future_t *f2,
                                        const char **file_data,
                                        int *file_len,
                                        const char *user_path)
{
    // Extract the data from reply
    if (flux_rpc_get_raw (f2, (const void **) file_data, file_len) < 0) {
        FLUX_LOG_ERR ("flux_rpc_get_raw(\"%s\") failed.\n", user_path);
        return -1;
    }
    return 0;
}

__attribute__((annotate("@critical_path()")))
#endif // DYAD_PERFFLOW
int dyad_stream_core::subscribe_via_flux (const char *user_path)
{
    int rc = -1;
    uint32_t owner_rank = 0u;
    flux_future_t *f1 = NULL;
    flux_future_t *f2 = NULL;
    const char *file_data = NULL;
    int file_len = 0;
    const char *odir = NULL;
    mode_t m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    FILE *of = NULL;

    const size_t topic_len = PATH_MAX;
    char topic [topic_len + 1];
    char file_path [PATH_MAX+1] = {'\0'};
    char file_path_copy [PATH_MAX+1] = {'\0'};

    memset (topic, '\0', topic_len + 1);
    gen_path_key (user_path, topic, topic_len, m_key_depth, m_key_bins);

    if (m_flux == nullptr)
        goto done;

    FLUX_LOG_INFO ("DYAD_SYNC CONS: subscribe_via_flux() for \"%s\" " \
                   "under namespace \"%s\".\n", \
                   topic, m_kvs_namespace.c_str ());

#if DYAD_PERFFLOW
    dyad_kvs_loopup (topic, &owner_rank);
#else
    // Look up key-value-store for the owner of a file
    f1 = flux_kvs_lookup (m_flux, m_kvs_namespace.c_str (),
                          FLUX_KVS_WAITCREATE, topic);
    if (f1 == NULL) {
        FLUX_LOG_ERR ("flux_kvs_lookup(%s) failed.\n", topic);
        goto done;
    }

    // Extract the value from the reply
    if (flux_kvs_lookup_get_unpack (f1, "i", &owner_rank) < 0) {
        FLUX_LOG_ERR ("flux_kvs_lookup_get_unpack() failed.\n");
        goto done;
    }
    flux_future_destroy (f1);
    f1 = NULL;
#endif // DYAD_PERFFLOW

    FLUX_LOG_INFO ( \
              "DYAD_SYNC CONS: flux_kvs_lookup(%s) identifies the owner %u\n", \
               topic, owner_rank);

    // If the owner is on the same storage, then no need to transfer the file.
    if (m_shared_storage || (owner_rank == m_rank)) {
        rc = 0;
        goto done;
    }

#if DYAD_PERFFLOW
   if (dyad_rpc_pack (&f2, owner_rank, user_path))
       goto done;
   if (dyad_rpc_get_raw (f2, &file_data, &file_len, user_path))
       goto done;
#else
    // Send the request to fetch a file
    if (!(f2 = flux_rpc_pack (m_flux, "dyad.fetch", owner_rank, 0,
                              "{s:s}", "upath", user_path))) {
        FLUX_LOG_ERR ("flux_rpc_pack({dyad.fetch %s})", user_path);
        goto done;
    }

    // Extract the data from reply
    if (flux_rpc_get_raw (f2, (const void **) &file_data, &file_len) < 0) {
        FLUX_LOG_ERR ("flux_rpc_get_raw(\"%s\") failed.\n", user_path);
        goto done;
    }
#endif // DYAD_PERFFLOW

    FLUX_LOG_INFO ("The size of file received: %d.\n", file_len);

    // Set output file path
    strncpy (file_path, m_dyad_path_cons.c_str(), PATH_MAX-1);
    concat_str (file_path, user_path, "/", PATH_MAX);
    strncpy (file_path_copy, file_path, PATH_MAX); // dirname modifies the arg
    file_path_copy[PATH_MAX] = '\0';

    // Create the directory as needed
    // TODO: Need to be consistent with the mode at the source
    odir = dirname (file_path_copy);
    m = (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH | S_ISGID);
    if ((strncmp (odir, ".", strlen(".")) != 0) &&
        (mkdir_as_needed (odir, m) < 0)) {
        FLUX_LOG_ERR ("Failed to create directory \"%s\".\n", odir);
        goto done;
    }

    // Write the file.
    of = fopen (file_path, "w");
    if (of == NULL) {
        FLUX_LOG_ERR ("Could not open to write file: %s\n", file_path);
        goto done;
    }
    if (fwrite (file_data, sizeof (char), (size_t) file_len, of)
        != (size_t) file_len) {
        FLUX_LOG_ERR ("Could not write file: %s\n", file_path);
        goto done;
    }
    // Close the file
    rc = ((fclose (of) == 0)? 0 : -1);

    flux_future_destroy (f2);
    f2 = NULL;

done:
    if (f1 != NULL) {
        flux_future_destroy (f1);
    }
    if (f2 != NULL) {
        flux_future_destroy (f2);
    }
    return rc;
}

#if DYAD_PERFFLOW
__attribute__((annotate("@critical_path()")))
int dyad_stream_core::dyad_kvs_commit (flux_kvs_txn_t *txn,
                                       flux_future_t **f,
                                       const char *user_path)
{
    if (!(*f = flux_kvs_commit (m_flux, m_kvs_namespace.c_str (), 0, txn))) {
        FLUX_LOG_ERR ("flux_kvs_commit(owner rank of %s = %u)\n", \
                       user_path, m_rank);
        return -1;
    }

    flux_future_wait_for (*f, -1.0);
    flux_future_destroy (*f);
    *f = NULL;
    flux_kvs_txn_destroy (txn);

    return 0;
}

__attribute__((annotate("@critical_path()")))
#endif // DYAD_PERFFLOW
int dyad_stream_core::publish_via_flux (const char *user_path)
{
    int rc = -1;
    flux_kvs_txn_t *txn = NULL;
    flux_future_t *f = NULL;

    const size_t topic_len = PATH_MAX;
    char topic [topic_len + 1];

    memset (topic, '\0', topic_len + 1);
    gen_path_key (user_path, topic, topic_len, m_key_depth, m_key_bins);

    if (m_flux == nullptr)
        goto done;

    FLUX_LOG_INFO ("DYAD_SYNC PROD: publish_via_flux() for \"%s\" " \
                   "under namespace \"%s\".\n", \
                   topic, m_kvs_namespace.c_str ());

    // Register the owner of the file into key-value-store
    if (!(txn = flux_kvs_txn_create ())) {
        FLUX_LOG_ERR ("flux_kvs_txn_create() failed.\n");
        goto done;
    }
    if (flux_kvs_txn_pack (txn, 0, topic, "i", m_rank) < 0) {
        FLUX_LOG_ERR ("flux_kvs_txn_pack(\"%s\",\"i\",%u) failed.\n", \
                       topic, m_rank);
        goto done;
    }

#if DYAD_PERFFLOW
    if (dyad_kvs_commit (txn, &f, user_path) < 0)
        goto done;
#else
    if (!(f = flux_kvs_commit (m_flux, m_kvs_namespace.c_str (), 0, txn))) {
        FLUX_LOG_ERR ("flux_kvs_commit(owner rank of %s = %u)\n", \
                       user_path, m_rank);
        goto done;
    }

    flux_future_wait_for (f, -1.0);
    flux_future_destroy (f);
    f = NULL;
    flux_kvs_txn_destroy (txn);
#endif // DYAD_PERFFLOW

    rc = 0;

done:
    return rc;
}


bool dyad_stream_core::open_sync (const char *path)
{
    IPRINTF ("DYAD_SYNC OPEN: enters sync (\"%s\").\n", path);

    int rc = 0;
    char upath [PATH_MAX] = {'\0'}; // path with the dyad_path prefix removed

    if (! is_dyad_consumer ()) {
        return true;
    }

    if (cmp_canonical_path_prefix (m_dyad_path_cons.c_str(),
                                   path, upath, PATH_MAX))
    {
      #ifdef TEST_WITHOUT_FLUX
        rc = 0;
      #else
        rc = subscribe_via_flux (upath); // annotate
      #endif
    } else {
        IPRINTF ("DYAD_SYNC OPEN: %s is not a prefix of %s.\n", \
                 m_dyad_path_cons.c_str(), path);
    }

    if (rc < 0) {
        DPRINTF ("DYAD_SYNC OPEN: failed sync (\"%s\").\n", path);
        return false;
    }

    IPRINTF ("DYAD_SYNC OEPN: exists sync (\"%s\").\n", path);
    return true;
}

bool dyad_stream_core::close_sync (const char *path)
{
    IPRINTF ("DYAD_SYNC CLOSE: enters sync (\"%s\").\n", path);

    int rc = 0;
    char upath [PATH_MAX] = {'\0'};

    if (! is_dyad_producer ()) {
        return true;
    }

    if (cmp_canonical_path_prefix (m_dyad_path_prod.c_str(),
                                   path, upath, PATH_MAX))
    {
      #if DYAD_SYNC_DIR
        sync_directory (path);
      #endif // DYAD_SYNC_DIR

      #ifdef TEST_WITHOUT_FLUX
        rc = 0;
      #else
        rc = publish_via_flux (upath);
      #endif
    } else {
        IPRINTF ("DYAD_SYNC CLOSE: %s is not a prefix of %s.\n", \
                 m_dyad_path_prod.c_str(), path);
    }

    if (rc < 0) {
        DPRINTF ("DYAD_SYNC CLOSE: failed sync (\"%s\").\n", path);
        return false;
    }

    IPRINTF ("DYAD_SYNC CLOSE: exists sync (\"%s\").\n", path);
    return true;
}

#if DYAD_SYNC_DIR
int dyad_stream_core::sync_directory (const char* path)
{ // Flush new directory entry https://lwn.net/Articles/457671/
    char path_copy [PATH_MAX+1] = {'\0'};
    int odir_fd = -1;
    char *odir = NULL;
    bool reenter = false;
    int rc = 0;

    strncpy (path_copy, path, PATH_MAX);
    odir = dirname (path_copy);

    if ((odir_fd = open_real (odir, O_RDONLY)) < 0) {
        IPRINTF ("Cannot open the directory \"%s\"\n", odir);
        rc = -1;
    } else {
        if (fsync (odir_fd) < 0) {
            IPRINTF ("Cannot flush the directory \"%s\"\n", odir);
            rc = -1;
        }
        if (close_real (odir_fd) < 0) {
            IPRINTF ("Cannot close the directory \"%s\"\n", odir);
            rc = -1;
        }
    }
    return rc;
}
#endif // DYAD_SYNC_DIR

} // end of namespace dyad

/*
 * vi: ts=4 sw=4 expandtab
 * y
 * y
 */
