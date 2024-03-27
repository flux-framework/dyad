#!/bin/bash
source ./dspaces-setup-env.sh 

# TODO startup dspaces_server

if [[ "${GENERATE_DATA}" == "1" ]]; then
# Generate Data for Workload
echo Generating DLIO Dataset
flux submit -o cpu-affinity=off -N $((NUM_NODES*BROKERS_PER_NODE)) --tasks-per-node=$((PPN/BROKERS_PER_NODE)) dlio_benchmark ${CONFIG_ARG} workload=${DLIO_WORKLOAD} ++workload.dataset.data_folder=${DLIO_DATA_DIR} ++workload.workflow.generate_data=True ++workload.workflow.train=False
GEN_PID=$(flux job last)
flux job attach ${GEN_PID}
echo "Run without Gen data to do training"
exit
fi

# Run Training
echo Running DLIO Training for ${DLIO_WORKLOAD}
flux submit -N $((NUM_NODES*BROKERS_PER_NODE)) -o cpu-affinity=on --tasks-per-node=$((PPN/BROKERS_PER_NODE)) dlio_benchmark ${CONFIG_ARG} workload=${DLIO_WORKLOAD} ++workload.dataset.data_folder=${DLIO_DATA_DIR} ++workload.workflow.generate_data=False ++workload.workflow.train=True
RUN_PID=$(flux job last)
flux job attach ${RUN_PID} > ${DYAD_DLIO_RUN_LOG} 2>&1
#cat ${DYAD_DLIO_RUN_LOG}
echo "Finished Executing check ${DYAD_DLIO_RUN_LOG} for output"


# TODO run terminate
