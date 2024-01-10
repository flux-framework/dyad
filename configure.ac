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
AM_CONDITIONAL([URPC], [test "x$enable_urpc" = "xyes"])
AC_ARG_ENABLE([perfflow],
    [AS_HELP_STRING([--enable-perfflow],
                    [enable performance measurement with PerfFlow Aspect])],
    [enable_perfflow=$withval],
    [enable_perfflow=no]
)
AM_CONDITIONAL([PERFFLOW], [test "x$enable_perfflow" = "xyes"])
AC_ARG_ENABLE([ucx],
    [AS_HELP_STRING([--enable-ucx],
                    [build the UCX-based data transport layer])],
    [enable_ucx=$withval],
    [enable_ucx=no]
)
AM_CONDITIONAL([UCX], [test "x$enable_ucx" = "xyes"])
AC_ARG_ENABLE([caliper],
    [AS_HELP_STRING([--enable-caliper],
                    [enable performance measurement with Caliper])],
    [enable_caliper=$withval],
    [enable_caliper=no]
)

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
# Find and get info for Flux-Core using pkg-config
AX_FLUX_CORE
# Check for Jansson 2.10 or higher (Required)
PKG_CHECK_MODULES([JANSSON],
    [jansson >= 2.10]
)
# Check for PerfFlow Aspect
AX_PERFFLOW_ASPECT([PERFFLOW],
    [pkg_check_perfflow_found=yes],
    [pkg_check_perfflow_found=no]
)
if test "x$with_perfflow" = "xyes" && test "x$pkg_check_perfflow_found" = "xno"; then
    AC_MSG_ERROR([requested PerfFlow Aspect support, but cannot find PerfFlow Aspect with pkg-config])
fi
DYAD_MOD_RPATH=""
# Check for UCX v1.6.0 or higher
PKG_CHECK_MODULES([UCX],
    [ucx >= 1.6.0],
    [pkg_check_ucx_found=yes],
    [pkg_check_ucx_found=no]
)
if test "x$enable_ucx" = "xyes" && test "x$pkg_check_ucx_found" = "xno"; then
    AC_MSG_ERROR([requested UCX support, but cannot find UCX with pkg-config])
fi
if test "x$enable_ucx" = "xyes"; then
    PKG_CHECK_VAR([UCX_LIBDIR],
        [ucx >= 1.6.0],
        [libdir],
        [],
        [AC_MSG_FAILURE([Could not find libdir for UCX])]
    )
    AS_IF([test "x$UCX_LIBDIR" = "x"],
        [AC_MSG_FAILURE([check_var succeeded, but value is incorrect])]
    )
    AS_IF([test "x$enable_ucx" = "xyes"],
        [
            if test -z "$DYAD_MOD_RPATH"; then
                DYAD_MOD_RPATH="$UCX_LIBDIR"
            else
                DYAD_MOD_RPATH="$DYAD_MOD_RPATH:$UCX_LIBDIR"
            fi
        ],
        []
    )
fi
PKG_CHECK_MODULES([CALIPER],
    [caliper],
    [pkg_check_caliper_found=yes],
    [pkg_check_caliper_found=no]
)
if test "x$enable_caliper" = "xyes" && test "x$pkg_check_caliper_found" = "xno"; then
    AC_MSG_ERROR([requested Caliper support, but cannot find Caliper with pkg-config])
fi
if test "x$enable_caliper" = "xyes"; then
    PKG_CHECK_VAR([CALIPER_LIBDIR],
        [caliper],
        [libdir],
        [],
        [AC_MSG_FAILURE([Could not find libdir for Caliper])]
    )
    AS_IF([test "x$CALIPER_LIBDIR" = "x"],
          [AC_MSG_FAILURE([check_var succeeded, but value is incorrect])]
    )
    AS_IF([test "x$enable_caliper" = "xyes"],
        [
            if test -z "$DYAD_MOD_RPATH"; then
                DYAD_MOD_RPATH="$CALIPER_LIBDIR"
            else
                DYAD_MOD_RPATH="$DYAD_MOD_RPATH:$CALIPER_LIBDIR"
            fi
        ],
        []
    )
fi
AM_CONDITIONAL([WITH_CALIPER], [test "x$enable_caliper" = "xyes"])

AS_IF([test -z "$DYAD_MOD_RPATH"],
    [],
    [DYAD_MOD_RPATH="-Wl,-rpath,$DYAD_MOD_RPATH"]
)
AC_SUBST([DYAD_MOD_RPATH])

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
    CFLAGS="$CFLAGS -DDYAD_FULL_DEBUG=1 -DDYAD_LOGGING_ON=1"
    CXXFLAGS="$CXXFLAGS -DDYAD_FULL_DEBUG=1 -DDYAD_LOGGING_ON=1"
fi

########################
# Configures Makefiles #
########################
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/perf/Makefile
                 src/utils/Makefile
                 src/utils/base64/Makefile
                 src/utils/libtap/Makefile
                 src/dtl/Makefile
                 src/core/Makefile
                 src/stream/Makefile
                 src/modules/Makefile
                 src/wrapper/Makefile])

#####################
# Complete Autoconf #
#####################
AC_OUTPUT
