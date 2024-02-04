#include <dyad/c_api/dyad_c.h>
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/common/dyad_rc.h>
#include <dyad/core/dyad_core.h>
#include <dyad/common/dyad_envs.h>
#include <dyad/utils/utils.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <libgen.h>  // dirname
#include <unistd.h>

static inline int is_wronly (int fd)
{
    int rc = fcntl (fd, F_GETFL);
    if (rc == -1)
        return -1;
    if ((rc & O_ACCMODE) == O_WRONLY)
        return 1;
    return 0;
}

int dyad_c_init (dyad_ctx_t** ctx)
{
    return (int)dyad_init_env (ctx);
}

int dyad_c_open (dyad_ctx_t* ctx, const char* path, int oflag, ...)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_STR ("path", "path");
    int mode = 0;

    if (oflag & O_CREAT) {
        va_list arg;
        va_start (arg, oflag);
        mode = va_arg (arg, int);
        va_end (arg);
    }
    
    if (is_path_dir(path)) {
        DYAD_LOG_INFO(ctx, "DYAD_C: skipping dyad_consume because path points to a directory");
        goto real_call;
    }

    DYAD_LOG_INFO (ctx, "DYAD_C: checking open mode");
    if ((oflag & O_ACCMODE) != O_RDONLY) {
        // TODO: make sure if the directory mode is consistent
        DYAD_LOG_INFO (ctx, "DYAD_C: skipping dyad_consume due to non-read open mode");
        goto real_call;
    }

    if (!(ctx && ctx->h) || (ctx && !ctx->reenter)) {
        DYAD_LOG_INFO (ctx, "DYAD_C: open sync not applicable for \"%s\".", path);
        goto real_call;
    }

    DYAD_LOG_INFO (ctx, "DYAD_C: enters open sync (\"%s\").", path);
    if (DYAD_IS_ERROR (dyad_consume (ctx, path))) {
        DYAD_LOG_ERROR (ctx, "DYAD_C: failed open sync (\"%s\").", path);
        goto real_call;
    }
    DYAD_LOG_INFO (ctx, "DYAD_C: exists open sync (\"%s\").", path);

real_call:;
    int ret = open (path, oflag, mode);
    if ((oflag & O_WRONLY || oflag & O_APPEND) && !is_path_dir (path)) {
        struct flock exclusive_lock;
        dyad_rc_t rc = dyad_excl_flock (ctx, ret, &exclusive_lock);
        if (DYAD_IS_ERROR (rc)) {
            dyad_release_flock (ctx, ret, &exclusive_lock);
        }
    }
    DYAD_C_FUNCTION_END ();
    return ret;
}

int dyad_c_close (dyad_ctx_t* ctx, int fd)
{
    DYAD_C_FUNCTION_START ();
    DYAD_C_FUNCTION_UPDATE_INT ("fd", fd);
    bool to_sync = false;
    char path[PATH_MAX + 1] = {'\0'};
    int rc = 0;

    if ((fd < 0) || (ctx == NULL) || (ctx->h == NULL) || !ctx->reenter) {
#if defined(IPRINTF_DEFINED)
        if (ctx == NULL) {
            IPRINTF (ctx, "DYAD_SYNC: close sync not applicable. (no context)\n");
        } else if (ctx->h == NULL) {
            IPRINTF (ctx, "DYAD_SYNC: close sync not applicable. (no flux)\n");
        } else if (!ctx->reenter) {
            IPRINTF (ctx, "DYAD_SYNC: close sync not applicable. (no reenter)\n");
        } else if (fd >= 0) {
            IPRINTF (ctx,
                     "DYAD_SYNC: close sync not applicable. (invalid file "
                     "descriptor)\n");
        }
#endif  // defined(IPRINTF_DEFINED)
        to_sync = false;
        goto real_call;
    }

    if (is_fd_dir (fd)) {
        // TODO: make sure if the directory mode is consistent
        goto real_call;
    }

    if (get_path (fd, PATH_MAX - 1, path) < 0) {
        DYAD_LOG_DEBUG (ctx, "DYAD_C: unable to obtain file path from a descriptor.\n");
        to_sync = false;
        goto real_call;
    }

    to_sync = true;

real_call:;  // semicolon here to avoid the error
    // "a label can only be part of a statement and a declaration is not a
    // statement"
    
    // TODO replace with an optional enable
    // fsync (fd);

#if DYAD_SYNC_DIR
    if (to_sync) {
        dyad_sync_directory (ctx, path);
    }
#endif  // DYAD_SYNC_DIR

    int wronly = is_wronly (fd);

    if (wronly == -1) {
        DYAD_LOG_WARN (ctx,
                       "Failed to check the mode of the file with fcntl: %s\n",
                       strerror (errno));
    }

    if (to_sync && wronly == 1) {
        rc = close (fd);
        if (rc != 0) {
            DYAD_LOG_ERROR (ctx, "Failed close (\"%s\").: %s\n", path, strerror (errno));
        }
        DYAD_LOG_DEBUG (ctx, "DYAD_C: enters close sync (\"%s\").\n", path);
        if (DYAD_IS_ERROR (dyad_produce (ctx, path))) {
            DYAD_LOG_ERROR (ctx, "DYAD_C: failed close sync (\"%s\").\n", path);
        }
        DYAD_LOG_DEBUG (ctx, "DYAD_C: exits close sync (\"%s\").\n", path);
        struct flock exclusive_lock;
        dyad_release_flock (ctx, fd, &exclusive_lock);
    } else {
        rc = close (fd);
    }
    DYAD_C_FUNCTION_END ();
    return rc;
}

int dyad_c_finalize (dyad_ctx_t** ctx)
{
    return (int)dyad_finalize (ctx);
}