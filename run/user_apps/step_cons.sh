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

# Set KVS namespace for file sharing context
unset FLUX_KVS_NAMESPACE
if [ "${DYAD_KVS_NS}" != "" ] ; then
    export FLUX_KVS_NAMESPACE=${DYAD_KVS_NS}
fi

if [ "${NO_MERLIN}" == "1" ] ; then
    date +"%T.%3N" > ${WORKSPACE}/time.${DYAD_CONTEXT}.txt
fi

# Compose the command
CMD="${PRELOAD} ${DYAD_OPT} ${SPECROOT}/user_apps/cons_test ${DYAD_PATH} ${BUFFERED} ${DYAD_CONTEXT} ${CHECK} ${ITER} ${FIXED_FILE_SIZE} ${USEC}"

# Execute the benchmark
if [ ${DRYRUN} -eq 0 ] ; then
    echo ${CMD} >  ${WORKSPACE}/runs_cons.${DYAD_CONTEXT}.out
    eval ${CMD} >> ${WORKSPACE}/runs_cons.${DYAD_CONTEXT}.out 2> ${WORKSPACE}/runs_cons.${DYAD_CONTEXT}.err
else
    echo ${CMD}
fi

if [ "${NO_MERLIN}" == "1" ] ; then
    date +"%T.%3N" >> ${WORKSPACE}/time.${DYAD_CONTEXT}.txt
    hostname > ${WORKSPACE}/hostname.${DYAD_CONTEXT}.txt
fi