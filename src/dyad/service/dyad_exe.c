#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

struct dyad_cli_args {
    char *prod_managed_path;
    char *dtl_mode;
    char *origin_path;
    bool debug;
};
typedef struct dyad_cli_args dyad_cli_args_t;

// global variable to store parsed command line arguments
static dyad_cli_args_t cli_args = {NULL, NULL, NULL, false};

typedef enum { INVALID_ACTION = -1, ACT_START = 0, ACT_STOP = 1, N_ACT = 2 } action_e;

static char *actions[N_ACT] = {"start", "stop"};

// global variable storing the action to perform
static action_e action = INVALID_ACTION;

// static dyad_resource_t resource;

static void usage (int status)
{
    printf (
        "\n"
        "Usage: dyad <command> [options...]\n"
        "\n"
        "<command> should be one of the following:\n"
        "  start       start the dyad service daemons\n"
        "  stop        stop the dyad service daemons\n");
    printf (
        "\n"
        "Common options:\n"
        "    -h, --help:  Show help.\n"
        "    -d, --debug: Enable debugging log message.\n");
    printf (
        "\n"
        "Command options for \"start\":\n"
        "    -p, --producer_managed_path:  Mandatory argument.\n"
        "                                  Path to the producer data directory.\n"
        "    -m, --mode:  DTL mode. Need an argument.\n"
        "                 'FLUX_RPC' (default), 'UCX', or 'MARGO'.\n"
        "    -i, --info_log: Specify the file into which to redirect\n"
        "                    info logging. Does nothing if DYAD was not\n"
        "                    configured with '-DDYAD_LOGGER=PRINTF'.\n"
        "                    Need a filename as an argument.\n"
        "    -e, --error_log: Specify the file into which to redirect\n"
        "                     error logging. Does nothing if DYAD was\n"
        "                     not configured with '-DDYAD_LOGGER=PRINTF'\n"
        "                     Need a filename as an argument.\n"
        "    -o, --origin_path: Fallback source path (e.g. on the parallel\n"
        "                       file system) used to lazily fill missing\n"
        "                       spans of a managed file on demand. Need a\n"
        "                       path as an argument. Omit to require files\n"
        "                       be fully staged upfront (default).\n");
    printf (
        "\n"
        "Command options for \"stop\":\n"
        "    No additional options.\n");

    exit (status);
}

static void parse_cmd_arguments (int argc, char **argv)
{
    int ch = 0;
    int optidx = 2;
    optind = 2;

    static struct option long_options[] = {{"help", no_argument, 0, 'h'},
                                           {"debug", no_argument, 0, 'd'},
                                           {"mode", required_argument, 0, 'm'},
                                           {"producer_managed_path", required_argument, 0, 'p'},
                                           {"info_log", required_argument, 0, 'i'},
                                           {"error_log", required_argument, 0, 'e'},
                                           {"origin_path", required_argument, 0, 'o'},
                                           {0, 0, 0, 0}};
    static char *short_options = "hdm:i:e:p:o:";

    while ((ch = getopt_long (argc, argv, short_options, long_options, &optidx)) >= 0) {
        switch (ch) {
            case 'h':
                usage (EXIT_SUCCESS);
                break;
            case 'd':
                cli_args.debug = true;
                break;
            case 'p':
                cli_args.prod_managed_path = strdup (optarg);
                break;
            case 'm':
                cli_args.dtl_mode = strdup (optarg);
                // TODO: check if the user specified dtl_mode is valid.
                break;
            case 'i':
                break;
            case 'e':
                break;
            case 'o':
                cli_args.origin_path = strdup (optarg);
                break;
            case '?':
                // getopt_long already printed an error message.
                break;
            default:
                usage (EXIT_FAILURE);
                break;
        }
    }
}

static int fork_exec_wait (char *const argv[])
{
    pid_t pid = fork ();
    if (pid < 0) {
        perror ("fork");
        return EXIT_FAILURE;
    }
    if (pid == 0) {
        execvp (argv[0], argv);
        perror ("execvp");
        exit (EXIT_FAILURE);
    }
    int status;
    if (waitpid (pid, &status, 0) < 0) {
        perror ("waitpid");
        return EXIT_FAILURE;
    }
    return WIFEXITED (status) ? WEXITSTATUS (status) : EXIT_FAILURE;
}

int dyad_start_service (dyad_cli_args_t *cli_args)
{
    // DYAD_INSTALL_LIBDIR is defined in dyad_config.h.in
    char dyad_module_path[PATH_MAX + 1] = {0};
    sprintf (dyad_module_path, "%s/dyad.so", DYAD_INSTALL_LIBDIR);

    // Fixed-size buffer sized for the largest possible combination of
    // flags below; grow this if another optional flag is added.
    char *argv[16];
    int i = 0;
    argv[i++] = "flux";
    argv[i++] = "exec";
    argv[i++] = "-r";
    argv[i++] = "all";
    argv[i++] = "flux";
    argv[i++] = "module";
    argv[i++] = "load";
    argv[i++] = dyad_module_path;
    if (cli_args->dtl_mode != NULL) {
        argv[i++] = "--mode";
        argv[i++] = cli_args->dtl_mode;
    }
    if (cli_args->origin_path != NULL) {
        argv[i++] = "--origin_path";
        argv[i++] = cli_args->origin_path;
    }
    argv[i++] = cli_args->prod_managed_path;
    argv[i++] = NULL;

    return fork_exec_wait (argv);
}

int dyad_stop_service (dyad_cli_args_t *cli_args)
{
    (void)cli_args;
    char *const argv[] = {"flux", "exec", "-r", "all", "flux", "module", "remove", "dyad", NULL};
    return fork_exec_wait (argv);
}

int main (int argc, char **argv)
{
    char *cmd = NULL;

    // Usage: dyad start|stop [options]
    if (argc < 2) {
        usage (EXIT_FAILURE);
    }

    // argv[0]: dyad
    // argv[1]: start|stop
    cmd = argv[1];

    for (int i = 0; i < N_ACT; i++) {
        if (strcmp (cmd, actions[i]) == 0) {
            action = (action_e)i;
            break;
        }
    }

    if (action == INVALID_ACTION) {
        usage (EXIT_FAILURE);
    }

    parse_cmd_arguments (argc, argv);

    if (action == ACT_START) {
        if (NULL == cli_args.prod_managed_path) {
            printf ("USAGE ERROR: producer managed directory (-p) is required!\n");
            usage (EXIT_FAILURE);
        }
        return dyad_start_service (&cli_args);
    } else if (action == ACT_STOP) {
        return dyad_stop_service (&cli_args);
    } else {
        fprintf (stderr, "USAGE ERROR: unhandled action %d\n", (int)action);
        return EXIT_FAILURE;
    }
}
