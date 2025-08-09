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
# flux mini run -N 2 -n 2 -o mpi=spectrum ./combined_test.sh

if [ -f args.sh ] ; then
    source ./args.sh
else
    echo "Cannot find argument parser. CWD=$d" >&2
    exit 1
fi

if [ -z ${DYAD_CONTEXT+x} ] ; then
    echo "DYAD_CONTEXT is not set"
    exit 1
fi

# Clean up the dyad directory
if [ ${DRYRUN} -eq 0 ] ; then
    pushd `dirname ${DYAD_PATH}` > /dev/null
    rm -rf ${DYAD_PATH} 2> /dev/null
    mkdir -p ${DYAD_PATH} 2> /dev/null
    popd > /dev/null
fi

# Set KVS namespace for file sharing context
unset DYAD_KVS_NAMESPACE
if [ "${DYAD_KVS_NS}" != "" ] ; then
    export DYAD_KVS_NAMESPACE=${DYAD_KVS_NS}
fi

# Compose the command
CMD="${PRELOAD} ${DYAD_OPT} ./combined_test ${DYAD_PATH} ${BUFFERED} ${DYAD_CONTEXT} ${CHECK} ${SYNC} ${RAND} ${ITER} ${FIXED_FILE_SIZE} ${USEC}"

# Execute the benchmark
if [ ${DRYRUN} -eq 0 ] ; then
    echo ${CMD}
    eval ${CMD}
else
    echo ${CMD}
fi
