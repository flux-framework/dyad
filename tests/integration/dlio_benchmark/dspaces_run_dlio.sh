#!/bin/bash
source ./dspaces-setup-env.sh 

# Startup dspaces_server
echo "## Config file for DataSpaces server
ndim = 3
dims = $MAX_DIM_LENGTH_FOR_FILES
max_versions = 1
num_apps = 1" > dataspaces.conf 

flux submit -N $NUM_NODES --cores=$(( NUM_NODES*16 )) \
    --tasks-per-node=$BROKERS_PER_NODE dspaces_server $DSPACES_HG_STRING

# Wait for DataSpaces's server to create conf.ds
sleep 1s
while [ ! -f conf.ds ]; do
    sleep 1s
done
# Give the server enough time to write the contents
sleep 3s

if [[ "${GENERATE_DATA}" == "1" ]]; then
# Generate Data for Workload
echo Generating DLIO Dataset
flux submit -o cpu-affinity=off -N $((NUM_NODES*BROKERS_PER_NODE)) --tasks-per-node=$((PPN/BROKERS_PER_NODE)) dlio_benchmark ${CONFIG_ARG} workload=${DLIO_WORKLOAD} ++workload.dataset.data_folder=${DLIO_DATA_DIR} ++workload.workflow.generate_data=True ++workload.workflow.train=False
GEN_PID=$(flux job last)
flux job attach ${GEN_PID}
echo "Run without Gen data to do training"
exit
fi

# Preload data into DataSpaces
flux run -N $NUM_NODES --tasks-per-node=1 python3 ./dspaces_preloader.py $DLIO_DATA_DIR/train

# Run Training
echo Running DLIO Training for ${DLIO_WORKLOAD}
flux submit -N $NUM_NODES --cores=$(( NUM_NODES*32 )) -o cpu-affinity=on --tasks-per-node=$PPN \
    dlio_benchmark ${CONFIG_ARG} workload=${DLIO_WORKLOAD} \
    ++workload.dataset.data_folder=${DLIO_DATA_DIR} \
    ++workload.workflow.generate_data=False ++workload.workflow.train=True
RUN_PID=$(flux job last)
flux job attach ${RUN_PID} > ${DSPACES_DLIO_RUN_LOG} 2>&1
#cat ${DYAD_DLIO_RUN_LOG}
echo "Finished Executing check ${DSPACES_DLIO_RUN_LOG} for output"

flux run --ntasks=1 terminator

rm dataspaces.conf conf.ds
