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

# Set KVS namespace for file sharing context
unset FLUX_KVS_NAMESPACE
if [ "${DYAD_KVS_NS}" != "" ] ; then
    export FLUX_KVS_NAMESPACE=${DYAD_KVS_NS}
fi

# Compose the command
CMD="${PRELOAD} ${DYAD_OPT} ./cons_test ${DYAD_PATH} ${BUFFERED} ${DYAD_CONTEXT} ${CHECK} ${ITER} ${FIXED_FILE_SIZE} ${USEC}"

# Execute the benchmark
if [ ${DRYRUN} -eq 0 ] ; then
    echo ${CMD}
    eval ${CMD}
else
    echo ${CMD}
fi
