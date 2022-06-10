#########################
# Autoconf requirements #
#########################
# AC_PREREQ([2.71])

################
# Package Info #
################
AC_INIT([dyad], [0.1.0])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_CONFIG_SRCDIR([README.md])
AC_CONFIG_HEADERS([config/config.h])
# TODO Look into replacing this. It's deprecated as of Autoconf 2
AC_CANONICAL_SYSTEM

#######################
# Initialize Automake #
#######################
AM_INIT_AUTOMAKE([foreign subdir-objects])

######################
# Initialize Libtool #
######################
LT_INIT([shared dlopen])

#######################
# Checks for programs #
#######################
AC_PROG_AWK
AC_PROG_CXX
# Ensure that the C++ compiler supports C++11 (not gnu++11)
AX_CXX_COMPILE_STDCXX([11], [noext], [mandatory])
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
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
AC_ARG_ENABLE([perfflow],
    [AS_HELP_STRING([--enable-perfflow],
                    [whether to enable PerfFlow Aspect performance measurement])],
    [enable_perfflow=$enableval],
    [enable_perfflow=no]
)

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
PKG_CHECK_MODULES([FLUXCORE], [flux-core], [], [])
if test "x$enable_perfflow" = xyes; then
    PKG_CHECK_MODULES([PERFFLOW],
        [perfflowaspect],
        [has_perfflow=yes],
        [has_perfflow=err]
    )
else
    has_perfflow=no
fi
if test "$has_perfflow" = "err"; then
    AC_MSG_WARN([You requested PerfFlow Aspect support, but we couldn't find PerfFlow! Continuing without PerfFlow!])
    has_perfflow=no
fi
AM_CONDITIONAL([HASPERFFLOW], [test "$has_perfflow" = "yes"])

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
AC_DEFINE([_POSIX_C_SOURCE], [200112L], [Set the version of the POSIX standard to use])

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
                 src/common/Makefile
                 src/common/libtap/Makefile
                 src/lib/Makefile
                 src/modules/Makefile
                 src/wrapper/Makefile])

#####################
# Complete Autoconf #
#####################
AC_OUTPUT
