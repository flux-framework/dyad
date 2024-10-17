#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

// clang-format off
#include <dyad/common/dyad_structures_int.h>
#include <dyad/utils/utils.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
// clang-format on

int main(int argc, char **argv) {
  char *prefix = NULL;
  char *can_prefix = NULL;
  char *path = NULL;
  bool ret = false;
  char upath[PATH_MAX] = {'\0'};

  if (argc < 3 || 4 < argc) {
    printf("Usage: %s prefix path [canonical_prefix]\n", argv[0]);
    return EXIT_FAILURE;
  }
  if (argc >= 3) {
    prefix = argv[1];
    path = argv[2];
  }
  if (argc == 4) {
    can_prefix = argv[3];
  }

  const size_t prefix_len = strlen(prefix);
  const size_t can_prefix_len = can_prefix ? strlen(can_prefix) : 0u;
  ;
  uint32_t prefix_hash = hash_str(prefix, DYAD_SEED);
  uint32_t can_prefix_hash = hash_str(can_prefix, DYAD_SEED);

  dyad_ctx_t ctx;
  ctx.cons_managed_path = prefix;
  ctx.cons_real_path = can_prefix;
  ctx.cons_managed_len = prefix_len;
  ctx.cons_real_len = can_prefix_len;
  ctx.cons_managed_hash = prefix_hash;
  ctx.cons_real_hash = can_prefix_hash;
  bool is_prod = false;

  ret = cmp_canonical_path_prefix(&ctx, is_prod, path, upath, PATH_MAX);
  if (!ret) {
    printf("path '%s' does not include prefix '%s' ('%s')\n", path, prefix,
           can_prefix);
    return EXIT_FAILURE;
  }
  printf("path '%s' include prefix '%s' (canonical form: '%s')\n", path, prefix,
         can_prefix);
  printf("'%s' is under '%s'\n", upath, prefix);

  return EXIT_SUCCESS;
}
