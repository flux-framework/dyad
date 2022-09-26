#########################
# Autoconf requirements #
#########################
# Adds a minimum required Autoconf version to be consistent with flux-sched
AC_PREREQ([2.63])

################
# Package Info #
################
AC_INIT([dyad], [0.1.0])
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
AC_ARG_VAR([LIBB64_DIR], [root directory for libb64])

########################
# Checks for libraries #
########################
# Check for the dl library (specifically, the "dlsym" function)
AC_CHECK_LIB([dl], [dlsym])
# FIXME: Replace 'main' with a function in '-ldyad_fstream':
# AC_CHECK_LIB([dyad_fstream], [main])
# FIXME: Replace 'main' with a function in '-ltap':
# AC_CHECK_LIB([tap], [main])
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
if test "x$enable_urpc" = "xyes"; then
    if test "x$LIBB64_DIR" = "x"; then
        #AC_MSG_ERROR([LIBB64_DIR env variable needs to be set for urpc support])
        AC_CHECK_HEADERS([b64/cencode.h b64/cdecode.h], [], \
            [AC_MSG_ERROR([Unable to find the headers for libb64. \
                          Set LIBB64_DIR env variable to the custom installation path \
                          of libb64 for urpc support])]
        )
        AC_SEARCH_LIBS([base64_encode_block], [b64], \
            [AC_MSG_NOTICE([enable_urpc = $enable_urpc])], \
            [AC_MSG_ERROR([Unable to find the base64_encode_block() function. \
                          Set LIBB64_DIR env variable to the custom installation path \
                          of libb64 for urpc support])]
        )
    else
        AC_MSG_NOTICE([enable_urpc = $enable_urpc])
        AC_MSG_NOTICE([LIBB64_DIR = $LIBB64_DIR])
        AC_ARG_VAR([LIBB64_CPPFLAGS], [header location option for libb64])
        AC_ARG_VAR([LIBB64_LIBADD], [linker option for libb64])
        AC_SUBST([LIBB64_CPPFLAGS], ["-I$LIBB64_DIR/include"])
        AC_SUBST([LIBB64_LIBADD], ["-L$LIBB64_DIR/src"])
    fi
else
    AC_MSG_NOTICE([enable_urpc = $enable_urpc])

fi

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
    unistd.h \
    libgen.h \
    dlfcn.h \
    sys/types.h \
    sys/stat.h \
    linux/limits.h \
    limits.h \
    stdlib.h \
    stdarg.h \
    stdbool.h \
    stddef.h \
    string.h
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
    memset \
    mkdir \
    mkpath \
    realpath \
    readlink \
    dirname \
    setenv \
    getenv \
    fsync \
    nanosleep \
)

#######################################
# Add any necessary compilation flags #
#######################################
if test "x$enable_debug" = xyes; then
    AC_SUBST([CPPFLAGS], ["$CPPFLAGS -DDYAD_FULL_DEBUG=1 -DDYAD_LOGGING_ON=1"])
    if test "x$enable_urpc" = "xyes"; then
        AC_SUBST([CPPFLAGS], ["$CPPFLAGS -DURPC_FULL_DEBUG=1 -DURPC_LOGGING_ON=1"])
    fi
fi

STRNCAT_SPECIFIC_CFLAGS=;
if `$CXX -v 2>&1 | grep -v icc | grep 'gcc version' >/dev/null 2>&1` ; then
  STRNCAT_SPECIFIC_CFLAGS="-Wstringop-overflow=0"
fi

AC_SUBST(STRNCAT_SPECIFIC_CFLAGS, $STRNCAT_SPECIFIC_CFLAGS)

########################
# Configures Makefiles #
########################
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/common/Makefile
                 src/common/libtap/Makefile
                 src/lib/Makefile
                 src/modules/Makefile
                 src/wrapper/Makefile])

#####################
# Complete Autoconf #
#####################
AC_OUTPUT

echo "
  $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix...........: $prefix
  Debug Build......: $debug
  C Compiler.......: $CC
  C++ Compiler.....: $CXX
  CFLAGS...........: $CFLAGS
  CPPFLAGS.......... $CPPFLAGS
  CXXFLAGS.......... $CXXFLAGS
  FLUX.............: $FLUX
  FLUX_VERSION.....: $($FLUX version | sed -n 's/libflux-core:\t*//p')
  FLUX_CORE_CFLAGS.: $FLUX_CORE_CFLAGS
  FLUX_CORE_LIBS...: $FLUX_CORE_LIBS
  LIBFLUX_VERSION..: $LIBFLUX_VERSION
  FLUX_PREFIX......: $FLUX_PREFIX
  LDFLAGS..........: $LDFLAGS
  LIBS.............: $LIBS
  Linker...........: $LD
"
