#########################
# Autoconf requirements #
#########################
# Adds a minimum required Autoconf version to be consistent with flux-sched
AC_PREREQ([2.63])

################
# Package Info #
################
AC_INIT([dyad],
        m4_esyscmd([git describe --always --tags | awk '/.*/ {sub(/^v/, ""); printf "%s",$1; exit}']))
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_CONFIG_SRCDIR([README.md])

#######################
# Initialize Automake #
#######################
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_SILENT_RULES([yes])

######################
# Initialize Libtool #
######################
LT_INIT([shared dlopen])

###################################################
# Set the prefix to the prefix of Flux by default #
###################################################
# If the "flux" program is in PATH, set the default prefix to be
# the parent of the directory containing "flux" (i.e., the flux-core prefix).
# If the "flux" program is not found, use the value of AC_PREFIX_DEFAULT (usually /usr/local)
# as the default prefix.
# In either case, the prefix can be overwritten with the "--prefix" flag.
AC_PREFIX_PROGRAM([flux])

#######################
# Checks for programs #
#######################
AC_DEFINE([_GNU_SOURCE], 1,
          [Define _GNU_SOURCE so that we get all necessary prototypes])
AC_PROG_AWK
AC_PROG_CXX
# Ensure that the C++ compiler supports C++11 (not gnu++11)
AX_CXX_COMPILE_STDCXX([11], [noext], [mandatory])
AC_PROG_CC_C99
# TODO remove if/when we require Automake to be greater than or equal to version 1.14
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
PKG_PROG_PKG_CONFIG

#################################################
# Checks for additional configure-time features #
#################################################
AC_ARG_ENABLE([dyad-debug],
    [AS_HELP_STRING([--enable-dyad-debug],
                    [whether to include debugging prints in DYAD])],
    [enable_debug=$enableval],
    [enable_debug=no]
)
AC_ARG_ENABLE([urpc],
    [AS_HELP_STRING([--enable-urpc],
                    [enable compilation and install of URPC])],
    [enable_urpc=$enableval],
    [enable_urpc=no]
)
AC_ARG_ENABLE([perfflow],
    [AS_HELP_STRING([--enable-perfflow],
                    [enable performance measurement with PerfFlow Aspect])],
    [enable_perfflow=$enableval],
    [enable_perfflow=no]
)
# TODO Add support for libb64 back once base64 encoding/decoding is fully complete
# AC_ARG_VAR([LIBB64_DIR], [root directory for libb64])

#############################################
# Define PKG_CHECK_VAR if it does not exist #
#############################################

# Macro is copied from https://github.com/pkgconf/pkgconf/blob/master/pkg.m4
# with minor modifications to change comments from using 'dnl' to '#'
m4_ifndef([PKG_CHECK_VAR], [
# PKG_CHECK_VAR(VARIABLE, MODULE, CONFIG-VARIABLE,
# [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# -------------------------------------------
# Since: 0.28
#
# Retrieves the value of the pkg-config variable for the given module.
AC_DEFUN([PKG_CHECK_VAR],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1], [value of $3 for $2, overriding pkg-config])dnl
_PKG_CONFIG([$1], [variable="][$3]["], [$2])
AS_VAR_COPY([$1], [pkg_cv_][$1])
AS_VAR_IF([$1], [""], [$5], [$4])dnl
])# End of PKG_CHECK_VAR
])

########################
# Checks for libraries #
########################
# Check for the dl library (specifically, the "dlsym" function)
AC_CHECK_LIB([dl], [dlsym])
# Check for UCX v1.6.0 or higher (Required)
# TODO: make UCX optional based on a new AC_ARG_ENABLE flag
PKG_CHECK_MODULES([UCX],
    [ucx >= 1.6.0]
)
PKG_CHECK_VAR([UCX_LIBDIR],
    [ucx >= 1.6.0],
    [libdir],
    [],
    [AC_MSG_FAILURE([Could not find libdir for UCX])]
)
AS_IF([test "x$UCX_LIBDIR" = "x"],
    [AC_MSG_FAILURE([check_var succeeded, but value is incorrect])]
)
# Find and get info for Flux-Core using pkg-config
AX_FLUX_CORE
PKG_CHECK_MODULES([JANSSON],
    [jansson >= 2.10],
    [pkg_check_jansson_found=yes],
    [pkg_check_jansson_found=no]
)
if test "x$enable_urpc" = "xyes" && test "x$pkg_check_jansson_found" = "xno"; then
    AC_MSG_ERROR([jansson cannot be located, but it is required when '--enable-urpc' is provided])
fi
AM_CONDITIONAL([URPC], [test "x$enable_urpc" = "xyes"])
AX_PERFFLOW_ASPECT([PERFFLOW],
    [pkg_check_perfflow_found=yes],
    [pkg_check_perfflow_found=no]
)
if test "x$enable_perfflow" = "xyes" && test "x$pkg_check_perfflow_found" = "xno"; then
    AC_MSG_ERROR([requested PerfFlow Aspect support, but cannot find PerfFlow Aspect with pkg-config])
fi
AM_CONDITIONAL([PERFFLOW], [test "x$enable_perfflow" = "xyes"])

###########################
# Checks for header files #
###########################
AC_HEADER_STDC
AC_CHECK_HEADERS( \
    fcntl.h \
    stdint.h \
    [sys/param.h] \
    unistd.h \
)

#################################################################
# Checks for typedefs, structures, and compiler characteristics #
#################################################################
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
# Defining _POSIX_C_SOURCE for now
# TODO look into side effects of setting this on non-Linux OSes (e.g., macOS)
AC_DEFINE([_POSIX_C_SOURCE], 200112L, [Set the version of the POSIX standard to use])

################################
# Checks for library functions #
################################
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS( \
    clock_gettime \
    getcwd \
    memset \
    mkdir \
    realpath \
    regcomp \
    setenv \
    strerror \
)

#######################################
# Add any necessary compilation flags #
#######################################
if test "x$enable_debug" = xyes; then
    CPPFLAGS="$CPPFLAGS -DDYAD_FULL_DEBUG=1 -DDYAD_LOGGING_ON=1"
fi

########################
# Configures Makefiles #
########################
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/utils/Makefile
                 src/utils/base64/Makefile
                 src/utils/libtap/Makefile
                 src/core/Makefile
                 src/stream/Makefile
                 src/modules/Makefile
                 src/wrapper/Makefile])

#####################
# Complete Autoconf #
#####################
AC_OUTPUT
