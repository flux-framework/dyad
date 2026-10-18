/**
 * @file test_murmur3.c
 * @brief Command-line test utility for @c gen_path_key().
 *
 * @details
 * Generates and prints hierarchical KVS keys for one or more path strings
 * using a local static implementation of @c gen_path_key(). This is used
 * to inspect and verify the key structure produced by the hashing scheme
 * at a given depth and width before deploying it in the DYAD infrastructure.
 *
 * Reads @p depth, @p width, and one or more path strings from the
 * command-line arguments and calls the local @c gen_path_key() to generate
 * a KVS key for each path. Each input path and its corresponding generated
 * key are printed to @c stdout as a tab-separated pair:
 * @code
 *   <original_path>\t<generated_key>
 * @endcode
 *
 * Usage:
 * @code
 *   test_murmur3 <depth> <width> <str1> [str2 [str3 ...]]
 * @endcode
 *
 * Arguments:
 *  - @c argv[1]: number of hash levels (depth).
 *  - @c argv[2]: number of buckets per level (width).
 *  - @c argv[3..]: path strings to generate keys for.
 *
 * @retval EXIT_SUCCESS  Keys were successfully generated and printed for
 *                       all provided path strings.
 * @retval EXIT_FAILURE  Fewer than 4 arguments were provided.
 *
 * @note Strings shorter than 128 bytes are padded with @c '@' characters
 *       to 128 bytes before hashing to improve hash distribution for short
 *       paths. Strings of 128 bytes or longer are hashed as-is.
 * @note The local @c gen_path_key() is the exact copy of one defined in
 *       client/dyad_client.c. It is copied here to avoid linking.
 *       Both implementations should be kept in sync.
 *
 * This is a standalone test executable and is not part of the DYAD library.
 */
#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dyad/utils/murmur3.h"

static int gen_path_key (const char *DYAD_RESTRICT str,
                         char *DYAD_RESTRICT path_key,
                         const size_t len,
                         const uint32_t depth,
                         const uint32_t width)
{
    static const uint32_t seeds[10] =
        {104677u, 104681u, 104683u, 104693u, 104701u, 104707u, 104711u, 104717u, 104723u, 104729u};

    uint32_t seed = 57u;
    uint32_t hash[4] = {0u};  // Output for the hash
    char buf[256] = {'\0'};
    size_t cx = 0ul;
    int n = 0;
    const char *str_long = str;
    size_t str_len = 0ul;

    if (str == NULL || path_key == NULL || len == 0ul) {
        return -1;
    }

    str_len = strlen (str);
    if (str_len == 0ul) {
        return -1;
    }

    path_key[0] = '\0';

    // Just append the string so that it can be as large as 128 bytes.
    if (str_len < 128ul) {
        memcpy (buf, str, str_len);
        memset (buf + str_len, '@', 128ul - str_len);
        buf[128u] = '\0';
        str_len = 128ul;
        str_long = buf;
    }

    for (uint32_t d = 0u; d < depth; d++) {
        seed += seeds[d % 10];
        MurmurHash3_x64_128 (str_long, str_len, seed, hash);
        uint32_t bin = (hash[0] ^ hash[1] ^ hash[2] ^ hash[3]) % width;
        n = snprintf (path_key + cx, len - cx, "%x.", bin);
        // n = snprintf (path_key + cx, len - cx, "%x%x%x%x.", hash[0], hash[1], hash[2], hash[3]);
        cx += n;
        if (cx >= len || n < 0) {
            return -1;
        }
    }
    n = snprintf (path_key + cx, len - cx, "%s", str);
    if (cx + n >= len || n < 0) {
        return -1;
    }

    return 0;
}

int main (int argc, char **argv)
{
    if (argc < 4) {
        printf ("Usage: %s depth width str1 [str2 [str3 ...]]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int depth = atoi (argv[1]);
    int width = atoi (argv[2]);
    for (int i = 3; i < argc; i++) {
        char path_key[PATH_MAX + 1] = {'\0'};
        gen_path_key (argv[i], path_key, PATH_MAX, depth, width);
        printf ("%s\t%s\n", argv[i], path_key);
    }

    return EXIT_SUCCESS;
}
