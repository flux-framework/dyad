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

EXP_OPTS="--kvs_namespace test"

if [ -f args.sh ] ; then
    source ./args.sh ${EXP_OPTS}
else
    echo "Cannot find argument parser. CWD=$d" >&2
    exit 1
fi

if [ -z ${DYAD_CONTEXT+x} ] ; then
    echo "DYAD_CONTEXT is not set"
    #exit 1
    DYAD_CONTEXT="dyad"
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
CMD="${PRELOAD} ${DYAD_OPT} ./prod_test ${DYAD_PATH} ${BUFFERED} ${DYAD_CONTEXT} ${ITER} ${FIXED_FILE_SIZE} ${USEC}"

# Execute the benchmark
if [ ${DRYRUN} -eq 0 ] ; then
    echo ${CMD}
    eval ${CMD}
else
    echo ${CMD}
fi
