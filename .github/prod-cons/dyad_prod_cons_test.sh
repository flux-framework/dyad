#!/bin/bash

this_script_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd)

source $this_script_dir/prod_cons_argparse.sh

echo "Creating namespace for DYAD"
flux kvs namespace create ${DYAD_KVS_NAMESPACE}

flux resource list

if [ "${DYAD_PATH}" == "" ] ; then
    DYAD_PATH="dir"
fi
export DYAD_PATH_CONSUMER=${DYAD_PATH}_consumer
export DYAD_PATH_PRODUCER=${DYAD_PATH}_producer


echo "Running Consumer job"
# flux submit --nodes 1 --exclusive --env=DYAD_KVS_NAMESPACE=${DYAD_KVS_NAMESPACE} --env=DYAD_DTL_MODE=${DYAD_DTL_MODE} --env=DYAD_PATH_CONSUMER=$DYAD_PATH_CONSUMER ${GITHUB_WORKSPACE}/.github/prod-cons/dyad_consumer.sh $mode
flux submit --nodes 1 --exclusive -t 10 --env=DYAD_KVS_NAMESPACE=${DYAD_KVS_NAMESPACE} --env=DYAD_DTL_MODE=${DYAD_DTL_MODE} ${GITHUB_WORKSPACE}/.github/prod-cons/dyad_consumer.sh $mode
CONS_PID=$(flux job last)
# Will block terminal until done
echo "Running Producer job"
# flux submit --nodes 1 --exclusive --env=DYAD_KVS_NAMESPACE=${DYAD_KVS_NAMESPACE} --env=DYAD_DTL_MODE=${DYAD_DTL_MODE} --env=DYAD_PATH_PRODUCER=$DYAD_PATH_PRODUCER ${GITHUB_WORKSPACE}/.github/prod-cons/dyad_producer.sh $mode
flux submit --nodes 1 --exclusive -t 10 --env=DYAD_KVS_NAMESPACE=${DYAD_KVS_NAMESPACE} --env=DYAD_DTL_MODE=${DYAD_DTL_MODE} ${GITHUB_WORKSPACE}/.github/prod-cons/dyad_producer.sh $mode
PROD_PID=$(flux job last)
flux jobs -a
flux job attach $PROD_PID
flux job attach $CONS_PID

flux kvs namespace remove ${DYAD_KVS_NAMESPACE}
flux exec -r all flux module remove dyad 2> /dev/null
flux exec -r all rm -rf ${DYAD_PATH_CONSUMER} ${DYAD_PATH_PRODUCER}
