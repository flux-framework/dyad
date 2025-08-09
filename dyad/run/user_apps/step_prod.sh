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

if [ ${DYAD_ARGS_PARSER_PATH} == "" ] ; then
    echo "The environment variable DYAD_ARGS_PARSER_PATH is not set."
    exit 1
fi

if [ `basename ${DYAD_ARGS_PARSER_PATH}` != "args.sh" ] || \
   [ ! -f ${DYAD_ARGS_PARSER_PATH} ]
then
    echo "Cannot find argument parser." >&2
    exit 1
fi

source ${DYAD_ARGS_PARSER_PATH}

if [ -z ${DYAD_CONTEXT+x} ] ; then
    echo "DYAD_CONTEXT is not set"
    exit 1
fi

# Clean up the dyad directory
#if [ ${DRYRUN} -eq 0 ] ; then
#    pushd `dirname ${DYAD_PATH}` > /dev/null
#    rm -rf ${DYAD_PATH} 2> /dev/null
#    mkdir -p ${DYAD_PATH} 2> /dev/null
#    popd > /dev/null
#fi

# Set KVS namespace for file sharing context
unset DYAD_KVS_NAMESPACE
if [ "${DYAD_KVS_NS}" != "" ] ; then
    export DYAD_KVS_NAMESPACE=${DYAD_KVS_NS}
fi

if [ "${NO_MERLIN}" == "1" ] ; then
    date +"%T.%3N" > ${WORKSPACE}/time.${DYAD_CONTEXT}.txt
fi

# Compose the command
CMD="${PRELOAD} ${DYAD_OPT} ${SPECROOT}/user_apps/prod_test ${DYAD_PATH} ${BUFFERED} ${DYAD_CONTEXT} ${ITER} ${FIXED_FILE_SIZE} ${USEC}"

# Execute the benchmark
if [ ${DRYRUN} -eq 0 ] ; then
    echo ${CMD} >  ${WORKSPACE}/runs_prod.${DYAD_CONTEXT}.out
    eval ${CMD} >> ${WORKSPACE}/runs_prod.${DYAD_CONTEXT}.out
else
    echo ${CMD}
fi

if [ "${NO_MERLIN}" == "1" ] ; then
    date +"%T.%3N" >> ${WORKSPACE}/time.${DYAD_CONTEXT}.txt
    hostname > ${WORKSPACE}/hostname.${DYAD_CONTEXT}.txt
fi
