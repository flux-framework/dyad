#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#define DYAD_UTIL_LOGGER 1
#define DYAD_LOGGER_FLUX 1

#include <inttypes.h>

#if defined(__cplusplus)
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#else
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#endif  // defined(__cplusplus)

#include <fcntl.h>
#include <getopt.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>

#include <dyad/common/dyad_dtl.h>
#include <dyad/common/dyad_envs.h>
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures.h>

/** This is a temporary measure until environment variable based initialization
 *  is implemented */
static dyad_dtl_mode_t get_dtl_mode_env ()
{
    char* e = NULL;

    size_t dtl_mode_env_len = 0ul;

    if ((e = getenv (DYAD_DTL_MODE_ENV))) {
        dtl_mode_env_len = strlen (e);
        if (strncmp (e, "FLUX_RPC", dtl_mode_env_len) == 0) {
            DYAD_LOG_STDERR ("DYAD MOD: FLUX_RPC DTL mode found in environment\n");
            return DYAD_DTL_FLUX_RPC;
        } else if (strncmp (e, "UCX", dtl_mode_env_len) == 0) {
            DYAD_LOG_STDERR ("DYAD MOD: UCX DTL mode found in environment\n");
            return DYAD_DTL_UCX;
        } else {
            DYAD_LOG_STDERR ("DYAD MOD: Invalid env %s = %s. Defaulting to %s\n",
                             DYAD_DTL_MODE_ENV,
                             e,
                             dyad_dtl_mode_name[DYAD_DTL_DEFAULT]);
            return DYAD_DTL_DEFAULT;
        }
    } else {
        DYAD_LOG_STDERR ("DYAD MOD: %s is not set. Defaulting to %s\n",
                         DYAD_DTL_MODE_ENV,
                         dyad_dtl_mode_name[DYAD_DTL_DEFAULT]);
        return DYAD_DTL_DEFAULT;
    }
    return DYAD_DTL_DEFAULT;
}

static void show_help (void)
{
    DYAD_LOG_STDOUT ("dyad module options and arguments\n");
    DYAD_LOG_STDOUT ("    -h, --help:  Show help.\n");
    DYAD_LOG_STDOUT ("    -d, --debug: Enable debugging log message.\n");
    DYAD_LOG_STDOUT (
        "    -m, --mode:  DTL mode. Need an argument.\n"
        "                 Either 'FLUX_RPC' (default) or 'UCX'.\n");
    DYAD_LOG_STDOUT (
        "    -i, --info_log: Specify the file into which to redirect\n"
        "                    info logging. Does nothing if DYAD was not\n"
        "                    configured with '-DDYAD_LOGGER=PRINTF'.\n"
        "                    Need a filename as an argument.\n");
    DYAD_LOG_STDOUT (
        "    -e, --error_log: Specify the file into which to redirect\n"
        "                     error logging. Does nothing if DYAD was\n"
        "                     not configured with '-DDYAD_LOGGER=PRINTF'\n"
        "                     Need a filename as an argument.\n");
}

static int opt_parse (dyad_ctx_t* ctx,
                      const unsigned broker_rank,
                      dyad_dtl_mode_t* dtl_mode,
                      int argc,
                      char** argv)
{
    size_t arglen = 0ul;
#ifndef DYAD_LOGGER_NO_LOG
    char log_file_name[PATH_MAX + 1] = {'\0'};
    char err_file_name[PATH_MAX + 1] = {'\0'};
    sprintf (log_file_name, "dyad_mod_%u.out", broker_rank);
    sprintf (err_file_name, "dyad_mod_%d.err", broker_rank);
#endif  // DYAD_LOGGER_NO_LOG
    *dtl_mode = DYAD_DTL_END;
    char* prod_managed_path = NULL;
    bool debug = false;
    (void)debug;

    while (1) {
        static struct option long_options[] = {{"help", no_argument, 0, 'h'},
                                               {"debug", no_argument, 0, 'd'},
                                               {"mode", required_argument, 0, 'm'},
                                               {"info_log", required_argument, 0, 'i'},
                                               {"error_log", required_argument, 0, 'e'},
                                               {0, 0, 0, 0}};
        /* getopt_long stores the option index here. */
        int option_index = 0;
        int c = -1;

        c = getopt_long (argc, argv, "hdm:i:e:", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) {
            DYAD_LOG_DEBUG (ctx, "DYAD_MOD: no more option\n");
            break;
        }
        DYAD_LOG_DEBUG (ctx, "DYAD_MOD: opt %c, index %d\n", (char)c, optind);

        switch (c) {
            case 'h':
                show_help ();
                break;
            case 'd':
                DYAD_LOG_DEBUG (ctx, "DYAD_MOD: 'debug' option -d \n");
                debug = true;
                break;
            case 'm':
                DYAD_LOG_DEBUG (ctx, "DYAD_MOD: DTL 'mode' option -m with value `%s'\n", optarg);
                arglen = strlen (optarg);
                if (strncmp (optarg, "FLUX_RPC", arglen) == 0) {
                    *dtl_mode = DYAD_DTL_FLUX_RPC;
                } else if (strncmp (optarg, "UCX", arglen) == 0) {
                    *dtl_mode = DYAD_DTL_UCX;
                } else {
                    DYAD_LOG_ERROR (ctx, "DYAD_MOD: Invalid DTL 'mode' (%s) provided\n", optarg);
                    *dtl_mode = DYAD_DTL_END;
                    show_help ();
                    return DYAD_RC_SYSFAIL;
                }
                break;
            case 'i':
#ifndef DYAD_LOGGER_NO_LOG
                DYAD_LOG_DEBUG (ctx, "DYAD_MOD: 'info_log' option -i with value `%s'\n", optarg);
                sprintf (log_file_name, "%s_%u.out", optarg, broker_rank);
#endif  // DYAD_LOGGER_NO_LOG
                break;
            case 'e':
#ifndef DYAD_LOGGER_NO_LOG
                DYAD_LOG_DEBUG (ctx, "DYAD_MOD: 'error_log' option -e with value `%s'\n", optarg);
                sprintf (err_file_name, "%s_%d.err", optarg, broker_rank);
#endif  // DYAD_LOGGER_NO_LOG
                break;
            case '?':
                /* getopt_long already printed an error message. */
                break;
            default:
                DYAD_LOG_DEBUG (ctx, "DYAD_MOD: option parsing failed %d\n", c);
                return DYAD_RC_SYSFAIL;
        }
    }

#ifndef DYAD_LOGGER_NO_LOG
    DYAD_LOG_STDOUT_REDIRECT (log_file_name);
    DYAD_LOG_STDERR_REDIRECT (err_file_name);
#endif  // DYAD_LOGGER_NO_LOG

    if (*dtl_mode == DYAD_DTL_END) {
        DYAD_LOG_DEBUG (ctx, "DYAD_MOD: Did not find DTL 'mode' option");
        *dtl_mode = get_dtl_mode_env ();
    }

    DYAD_LOG_DEBUG (ctx, "DYAD_MOD: optind %d argc %d\n", optind, argc);
    /* Print any remaining command line arguments (not options). */
    while (optind < argc) {
        DYAD_LOG_DEBUG (ctx, "DYAD_MOD: positional arguments %s\n", argv[optind]);
        prod_managed_path = argv[optind++];
    }
    if (prod_managed_path) {
        const size_t prod_path_len = strlen (prod_managed_path);
        const char* tmp_ptr = prod_managed_path;
        prod_managed_path = (char*)malloc (prod_path_len + 1);
        if (prod_managed_path == NULL) {
            DYAD_LOG_ERROR (ctx,
                            "DYAD_MOD: Could not allocate buffer for "
                            "Producer managed path!\n");
            return DYAD_RC_SYSFAIL;
        }
        strncpy (prod_managed_path, tmp_ptr, prod_path_len + 1);
        DYAD_LOG_INFO (ctx,
                       "DYAD_MOD: Loading DYAD Module with Path %s and DTL Mode %s",
                       prod_managed_path,
                       dyad_dtl_mode_name[*dtl_mode]);
    }
    DYAD_LOG_INFO (NULL, "DYAD_MOD: debug flag set to %s\n", debug ? "true" : "false");
    DYAD_LOG_INFO (NULL, "DYAD_MOD: prod_managed_path set to %s\n", prod_managed_path);
    return DYAD_RC_OK;
}

int main (int argc, char** argv)
{
    dyad_dtl_mode_t dtl_mode = DYAD_DTL_DEFAULT;
    uint32_t broker_rank = 0u;

    DYAD_LOG_DEBUG (NULL, "DYAD_MOD: Parsing command line options");
    if (opt_parse (NULL, broker_rank, &dtl_mode, argc, argv) != DYAD_RC_OK) {
        DYAD_LOG_ERROR (NULL, "DYAD_MOD: Cannot parse command line arguments");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
