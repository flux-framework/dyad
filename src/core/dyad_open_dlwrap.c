#include "dyad_open_dlwrap.h"

#include <dlfcn.h>
#include <fcntl.h>

FILE *fopen_real(const char *path, const char *mode)
{
    typedef FILE *(*fopen_real_ptr_t) (const char *, const char *);
    fopen_real_ptr_t func_ptr = (fopen_real_ptr_t) dlsym (RTLD_NEXT, "fopen");
    char *error = NULL;

    if ((error = dlerror ())) {
        return NULL;
    }

    return (func_ptr (path, mode));
}

FILE *dyad_fopen(dyad_ctx_t *ctx, const char *path, const char *mode)
{
    if (ctx->intercept)
    {
        FILE* fobj = fopen_real(path, mode);
        if (fobj == NULL)
        {
            // TODO log error
        }
        return fobj;
    }
    return fopen(path, mode);
}

int open_real (const char *path, int oflag, ...)
{
    typedef int (*open_real_ptr_t) (const char *, int, mode_t, ...);
    open_real_ptr_t func_ptr = (open_real_ptr_t) dlsym (RTLD_NEXT, "open");
    char *error = NULL;

    if (dlerror ()) {
        return -1;
    }

    int mode = 0;

    if (oflag & O_CREAT)
    {
        va_list arg;
        va_start (arg, oflag);
        mode = va_arg (arg, int);
        va_end (arg);
    }

    return (func_ptr (path, oflag, mode));
}

int dyad_open(dyad_ctx_t *ctx, const char *path, int oflag, ...)
{
    int mode;
    if (oflag & O_CREAT)
    {
        va_list arg;
        va_start(arg, oflag);
        mode = va_arg(arg, int);
        va_end(arg);
    }
    if (ctx->intercept)
    {
        int fp = open_real(path, oflag, mode);
        if (fp < 0)
        {
            // TODO log error
        }
        return fp;
    }
    return open(path, oflag, mode);
}

int fclose_real(FILE *fp)
{
    typedef int (*fclose_real_ptr_t) (FILE *);
    fclose_real_ptr_t func_ptr = (fclose_real_ptr_t) dlsym (RTLD_NEXT, "fclose");
    char *error = NULL;

    if ((error = dlerror ())) {
        return EOF; // return the failure code
    }

    return func_ptr (fp);
}

int dyad_fclose(dyad_ctx_t *ctx, FILE *fp)
{
    if (ctx->intercept)
    {
        int rc = fclose_real(fp);
        if (rc < 0)
        {
            // TODO log error
        }
        return rc;
    }
    return fclose(fp);
}

int close_real (int fd)
{
    typedef int (*close_real_ptr_t) (int);
    close_real_ptr_t func_ptr = (close_real_ptr_t) dlsym (RTLD_NEXT, "close");
    char *error = NULL;

    if ((error = dlerror ())) {
        return -1; // return the failure code
    }

    return func_ptr (fd);
}

int dyad_close(dyad_ctx_t *ctx, int fd)
{
    if (ctx->intercept)
    {
        int rc = close_real(fd);
        if (rc < 0)
        {
            // TODO log error
        }
        return rc;
    }
    return close(fd);
}
