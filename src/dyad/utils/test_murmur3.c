#include "dyad/utils/murmur3.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

static int gen_path_key (const char* restrict str,
                         char* restrict path_key,
                         const size_t len,
                         const uint32_t depth,
                         const uint32_t width)
{
    static const uint32_t seeds[10] =
        {104677u, 104681u, 104683u, 104693u, 104701u, 104707u, 104711u, 104717u, 104723u, 104729u};

    uint32_t seed = 57u;
    uint32_t hash[4] = {0u};  // Output for the hash
    size_t cx = 0ul;
    int n = 0;
    size_t str_len = strlen (str);

    if (str == NULL || path_key == NULL || len == 0ul || str_len == 0ul) {
        return -1;
    }
    path_key[0] = '\0';

    const char* str_long = str;

#if 1
    // Strings shorter than 128 bytes collide. Especially, the hash value seems
    // to depend on the length of such a string.
    // For such a short string, we concatenate it as many times as needed to make
    // it longer than 128 bytes.
    if (str_len < 128ul) {
        char buf[PATH_MAX+1] = {'\0'};
        memcpy (buf, str, str_len);
        char* str_pos = buf + str_len;
        const char* const str_min = buf + 128ul;
        while (str_pos < str_min) {
            memcpy (str_pos, str, str_len);
            str_pos += str_len;
        };
        str_len = str_pos - buf;
        str_long = buf;
    }
#endif

    for (uint32_t d = 0u; d < depth; d++) {
        seed += seeds[d % 10];
        MurmurHash3_x64_128 (str_long, str_len, seed, hash);
        uint32_t bin = (hash[0] ^ hash[1] ^ hash[2] ^ hash[3]) % width;
        n = snprintf (path_key + cx, len - cx, "%x.", bin);
        //n = snprintf (path_key + cx, len - cx, "%x%x%x%x.", hash[0], hash[1], hash[2], hash[3]);
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


int main (int argc, char** argv)
{
    if (argc < 4) {
        printf ("Usage: %s depth width str1 [str2 [str3 ...]]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int depth = atoi (argv[1]);
    int width = atoi (argv[2]);
    for (int i = 3; i < argc; i++) {
        char path_key [PATH_MAX + 1] = {'\0'};
        gen_path_key (argv[i], path_key, PATH_MAX, depth, width);
        printf("%s\t%s\n", argv[i], path_key);
    }

    return EXIT_SUCCESS;
}
