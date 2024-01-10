#************************************************************
# Copyright 2021 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
#************************************************************

#!/bin/bash

################################################################
# Help message
################################################################

function help_message {
    local SCRIPT=$(basename ${0})
    local N=$(tput sgr0)    # Normal text
    local C=$(tput setf 4)  # Colored text
    cat << EOF
DYAD Benchmark parameters
Usage: ${SCRIPT} [options]
Options:
  ${C}-h, --help${N}              Display this help message and exit.
  ${C}-s${N}                      Synchronize producers and consumers explicitly by
                          either Merlin or MPI_Barrier. Without it, DYAD performs
                          synchronization.
  ${C}--sync${N} <var>            0 for DYAD sync and 1 for explicit sync.
  ${C}--depth${N} <val>           Depth of DYAD key hierarchy for looking up a path.
  ${C}--bins${N} <val>            Number of bins per layer of the path key hierarchy
  ${C}--wpath${N} <val>           Override path to the dyad wrapper.
  ${C}--mpath${N} <val>           Override path to the dyad module.
  ${C}--dpath${N} <val>           Override path to the dyad managed directory.
  ${C}--kvs_namespace${N} <val>   DYAD KVS namespace for file sharing context.
  ${C}--reset_kvs${N}             Reset FLUX KVS when starting up.
  ${C}--context${N} <val>         Sharing context name. This is used to creaate a
                          unique name of a directory over which a pair of a producer
                          and a consumer can share files. This is not needed for
                          single_step case where it is automatically assigned.
  ${C}--shared_fs${N}             Set DYAD to assume shared file system, and not to
                          transfer files. By default, it uses local storages.
  ${C}--iter${N} <val>            Number of iterations of sharing a set of files between
                          a producer and consumer pair.
  ${C}--buffered${N} <val>        Whether to use buffered I/O (fopen/fclose) or not
                          (open/close). 1 for the former, and 0 for the latter.
  ${C}--fixed_fsize${N} <val val> Use a set of a given number of files of a given size
  ${C}--rand${N}                  0 to disable random pairing in combined_test (default).
                          A positive integer for seeding RNG.
  ${C}--usec${N}                  Imitate the compute load by sleeping for the given
                          microseconds (default 0). It should not exceed 1000000.
  ${C}--sync_start${N} <val>      Sychronize the start of user tasks to help performance analysis.
                          Number of user tasks to synchronize.
  ${C}-c, --check${N}             Consumer verifies if the contents of files are correct.

  ${C}-d, --dryrun${N}            Show the final command string without executing it.
  ${C}-v, --verbose${N}           Print parameters and variables set up.
EOF
}

################################################################
# Default values
################################################################

# DYAD specific parameters
SYNC=0           # Explicit Syncrhonization. If set to 0, DYAD takes care of
                 # synchronization as well as file transfer.
DYAD_DEPTH=2     # Depth of DYAD key hierarchy for looking up a path
DYAD_BINS=256    # Number of bins per layer of the path key hierarchy
DYAD_WRAPPER=""  # To override path to the DYAD wrapper
DYAD_PATH=""     # Root of the DYAD Managed file directiry
DYAD_KVS_NS=""   # KVS namespace for file sharing context
DYAD_SHARED_FS=0 # Set DYAD to assume shared file system

# Benchmark specific parameters
ITER=1     # Number of iterations of sharing a set of files between
           # a producer and consumer pair
BUFFERED=1 # Use non-buffered I/O with open/close (0)
           # or buffered I/O with fclose/fopen (1)
RAND=0     # Whether to randomize producer-consumer pairing in combined
           # test. Use 0 to disable random pairing, or a positive integer
           # to seed the RNG used in random pairing.
CHECK=0    # Consumer verifies if the contents of files are correct

# Misc. parameters and variables
VERBOSE=0  # Print parameter setup
DRYRUN=0   # Show the final command string without executing it
CLUSTER=$(hostname | sed 's/\([a-zA-Z][a-zA-Z]*\)[0-9]*/\1/g')


################################################################
# Parse command-line arguments
################################################################

while :; do
    case ${1} in
        -h|--help)
            # Help message
            help_message
            exit 0
            ;;
        -s)
            # Explicit synchronization of producers and consumers
            SYNC=1
            ;;
        --sync)
            # 1 for explicit synchronization of producers and consumers
            # 0 for implicit synchronization by DYAD
            if [ -n "${2}" ]; then
                SYNC=${2}
                if [ ${SYNC} -ne 0 ] && [ ${SYNC} -ne 1 ] ; then
                  echo "\"${1}\" option requires an argument that is either 0 or 1" >&2
                  exit 1
                fi
                shift
            else
                echo "\"${1}\" option requires an argument that is either 0 or 1" >&2
                exit 1
            fi
            ;;
        --depth)
            # Depth of DYAD key hierarchy for looking up a path
            if [ -n "${2}" ]; then
                DYAD_DEPTH=${2}
                declare -i DYAD_DEPTH
                if [ ${DYAD_DEPTH} -lt 1 ] ; then
                  echo "\"${1}\" option requires a positive integer argument" >&2
                  exit 1
                fi
                shift
            else
                echo "\"${1}\" option requires a positive integer argument" >&2
                exit 1
            fi
            ;;
        --bins)
            # Number of bins per layer of the path key hierarchy
            if [ -n "${2}" ]; then
                DYAD_BINS=${2}
                declare -i DYAD_BINS
                if [  ${DYAD_BINS} -lt 1 ] ; then
                  echo "\"${1}\" option requires a positive integer argument" >&2
                  exit 1
                fi
                shift
            else
                echo "\"${1}\" option requires a positive integer argument" >&2
                exit 1
            fi
            ;;
        --wpath)
            # Override the path to the DYAD wrapper
            if [ -n "${2}" ]; then
                DYAD_WRAPPER=${2}
                shift
            else
                echo "\"${1}\" option requires a directory name" >&2
                exit 1
            fi
            ;;
        --mpath)
            # Override the path to the DYAD module
            if [ -n "${2}" ]; then
                DYAD_MODULE=${2}
                shift
            else
                echo "\"${1}\" option requires a directory name" >&2
                exit 1
            fi
            ;;
        --dpath)
            # Root of the DYAD Managed file directiry
            if [ -n "${2}" ]; then
                DYAD_PATH=${2}
                shift
            else
                echo "\"${1}\" option requires a directory name" >&2
                exit 1
            fi
            ;;
        --kvs_namespace)
            # kvs namespace for the context of file sharing between producer and consumer
            if [ -n "${2}" ]; then
                DYAD_KVS_NS=${2}
                shift
            else
                echo "\"${1}\" option requires a namespace string" >&2
                exit 1
            fi
            ;;
        --reset_kvs)
            # reset_kvs when starting DYAD up.
            RESET_KVS=1
            ;;
        --context)
            # Sharing context name. This is used to creaate a unique name of
            # a directory over which a pair of a producer and a consumer can
            # share files. This is not needed for single_step case where it
            # is automatically assigned
            if [ -n "${2}" ]; then
                DYAD_CONTEXT=${2}
                shift
            else
                echo "\"${1}\" option requires an argument" >&2
                exit 1
            fi
            ;;
        --shared_fs)
            # Set DYAD to assume shared file system, and not to
            # transfer files. By default, it uses local storages.
            DYAD_SHARED_FS=1
            ;;
        --iter)
            # Number of iterations of sharing a set of files between a producer
            # and consumer pairs
            if [ -n "${2}" ]; then
                ITER=${2}
                declare -i ITER
                if [  ${ITER} -lt 1 ] ; then
                  echo "\"${1}\" option requires a positive integer argument" >&2
                  exit 1
                fi
                shift
            else
                echo "\"${1}\" option requires a positive integer argument" >&2
                exit 1
            fi
            ;;
        --buffered)
            # use non-buffered I/O with open/close(0) or
            # use buffered I/O with fclose/fopen(1)
            if [ -n "${2}" ]; then
                BUFFERED=${2}
                if [ ${BUFFERED} -ne 0 ] && [ ${BUFFERED} -ne 1 ] ; then
                  echo "\"${1}\" option requires an argument that is either 0 or 1" >&2
                  exit 1
                fi
                shift
            else
                echo "\"${1}\" option requires an argument that is either 0 or 1" >&2
                exit 1
            fi
            ;;
        --fixed_fsize)
            if [ -n "${2}" ] && [ -n "${3}" ] &&
               [ "${2}" -eq "${2}" ] 2> /dev/null &&
               [ "${3}" -eq "${3}" ] 2> /dev/null ; then
                NUM_FILES=${2}
                FILE_SIZE=${3}
                if [ ${NUM_FILES} -gt 0 ] && [ ${FILE_SIZE} -gt 0 ] ; then
                    FIXED_FILE_SIZE="${NUM_FILES} ${FILE_SIZE}"
                fi
                shift
                shift
            else
                echo "\"${1}\" option requires two numeric arguments" >&2
                exit 1
            fi
            ;;
        --rand)
            # Whether to randomize producer-consumer pairing in combined test
            if [ -n "${2}" ]; then
                RAND=${2}
                if [ ${RAND} -lt 0 ] ; then
                  echo "\"${1}\" option requires an argument that is a non-negative integer" >&2
                  exit 1
                fi
                shift
            else
                echo "\"${1}\" option requires an argument that is a non-negative integer" >&2
                exit 1
            fi
            ;;
        --usec)
            # microsecons to sleep for imitating the compute load
            if [ -n "${2}" ]; then
                USEC=${2}
                if [ ${USEC} -lt 0 ] || [ ${USEC} -gt 1000000 ] ; then
                  echo "\"${1}\" option requires an argument that is an integer in [0 1000000]" >&2
                  exit 1
                fi
                shift
            else
                echo "\"${1}\" option requires an argument that is an integer in [0 1000000]" >&2
                exit 1
            fi
            ;;
        -c|--check)
            # Consumer verifies if the contents of files are correct
            CHECK=1
            ;;
        -v|--verbose)
            # Print parameter setup
            VERBOSE=1
            ;;
        -d|--dryrun)
            # Show the final command string without executing it
            DRYRUN=1
            ;;
        -?*)
            # Unknown option
            echo "Unknown option (${1})" >&2
            exit 1
            ;;
        *)
            # Break loop if there are no more options
            break
    esac
    shift
done


################################################################
# Check and set parameters
################################################################

# Set dyad wrapper path
if [ "${DYAD_WRAPPER}" == "" ] ; then
    DYAD_WRAPPER=../dyad_wrapper.so
fi

# Set dyad module path
if [ "${DYAD_MODULE}" == "" ] ; then
    DYAD_MODULE=../../modules/dyad.so
fi

if [ ${SYNC} -eq 0 ] && [ ${DRYRUN} -eq 0 ] && [ ! -f "${DYAD_WRAPPER}" ] ; then
    echo "dyad wrapper '${DYAD_WRAPPER}' does not exist" >&2
    exit 1
fi


# Set dyad option
DYAD_OPT="DYAD_KEY_DEPTH=${DYAD_DEPTH} DYAD_KEY_BINS=${DYAD_BINS}"


# Set DYAD_PATH
if [ "${DYAD_PATH}" == "" ] ; then
    if [ "${CLUSTER}" == "lassen" ] ; then
        if [ "${SYNC}" == "0" ] ; then
            if [ "${DYAD_SHARED_FS}" == "1" ] ; then
                DYAD_PATH=/p/gpfs1/${USER}/dyad
            else
                DYAD_PATH=`df -h | grep bb | awk '{if (length($6)>2) {print $6; exit}}' | uniq`/dyad
            fi
        else
            DYAD_PATH=/p/gpfs1/${USER}/dyad
            DYAD_SHARED_FS=1
        fi
    else
        if [ "${SYNC}" == "0" ] ; then
            if [ "${DYAD_SHARED_FS}" == "1" ] ; then
                DYAD_PATH=/p/lustre2/${USER}/dyad
            else
                if [ "${CLUSTER}" == "catalyst" ] ; then
                    DYAD_PATH=/l/ssd/${USER}/dyad
                else
                    DYAD_PATH=/tmp/${USER}/dyad
                fi
            fi
        else
            DYAD_PATH=/p/lustre2/${USER}/dyad
            DYAD_SHARED_FS=1
        fi
    fi
fi

if [ "${DYAD_SHARED_FS}" == "1" ] ; then
    DYAD_OPT="${DYAD_OPT=} DYAD_SHARED_STORAGE=1"
fi

# Set PRELOAD
if [ "${SYNC}" == "0" ] ; then
    if [ "${LD_PRELOAD}" != "" ] ; then
        PRELOAD="LD_PRELOAD=${DYAD_WRAPPER}:${LD_PRELOAD}"
    else
        PRELOAD="LD_PRELOAD=${DYAD_WRAPPER}"
    fi
fi


################################################################
# Display parameters
################################################################

# Function to print variable
function print_var {
    echo "${1}=${!1}"
}

function print_all_vars {
  # Print parameters
  if [ ${VERBOSE} -ne 0 ]; then
    echo ""
    echo "----------------------"
    echo "DYAD parameters"
    echo "----------------------"
    print_var SYNC
    print_var DYAD_DEPTH
    print_var DYAD_BINS
    print_var DYAD_WRAPPER
    print_var DYAD_MODULE
    print_var DYAD_PATH
    print_var DYAD_KVS_NS
    print_var RESET_KVS
    print_var DYAD_CONTEXT
    print_var DYAD_SHARED_FS
    print_var DYAD_OPT
    echo ""
    echo "----------------------"
    echo "Benchmark parameters"
    echo "----------------------"
    print_var ITER
    print_var BUFFERED
    print_var RAND
    if [ "${USEC}" == "" ] ; then
        echo "Use the default sleep duration (0 usec)."
    else
        print_var USEC
    fi
    print_var CHECK
    if [ "${FIXED_FILE_SIZE}" == "" ] ; then
        echo "Use the default set of data files."
    else
        print_var NUM_FILES
        print_var FILE_SIZE
    fi
    echo ""
    echo "----------------------"
    echo "Misc."
    echo "----------------------"
    print_var VERBOSE
    print_var DRYRUN
    print_var PRELOAD
    echo ""
  fi
}

print_all_vars

if [ "${FIXED_FILE_SIZE}" == "" ] && [ "${USEC}" != "" ]  ; then
    FIXED_FILE_SIZE="0 0"
fi
