#!/bin/bash
echo "Creating namespace for DYAD"
flux kvs namespace create ${DYAD_KVS_NAMESPACE}

flux resource list

echo "Running Consumer job"
flux submit --nodes 1 --exclusive --env=DYAD_KVS_NAMESPACE=${DYAD_KVS_NAMESPACE} --env=DYAD_DTL_MODE=${DYAD_DTL_MODE} --env=DYAD_PATH_CONSUMER=$DYAD_PATH_CONSUMER ${GITHUB_WORKSPACE}/.github/prod-cons/dyad_consumer.sh
CONS_PID=$(flux job last)
# Will block terminal until done
echo "Running Producer job"
flux submit --nodes 1 --exclusive --env=DYAD_KVS_NAMESPACE=${DYAD_KVS_NAMESPACE} --env=DYAD_DTL_MODE=${DYAD_DTL_MODE} --env=DYAD_PATH_PRODUCER=$DYAD_PATH_PRODUCER ${GITHUB_WORKSPACE}/.github/prod-cons/dyad_producer.sh
PROD_PID=$(flux job last)
flux jobs -a
flux job attach $PROD_PID
flux job attach $CONS_PID

flux kvs namespace remove ${DYAD_KVS_NAMESPACE}