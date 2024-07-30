#!/bin/bash
source ./dspaces-setup-env.sh 

curr_dir=$(pwd)
if [ $# -ge 1 ]; then
    cd $1
fi

if [ $# -gt 1 ]; then
    export NUM_NODES=$2
fi
echo "Number of nodes is $NUM_NODES"

ulimit -c unlimited

# Startup dspaces_server
echo "Generating DataSpaces config file"
echo "## Config file for DataSpaces server
ndim = 3
dims = $NUM_SAMPLES_PER_FILE,$NUM_ROWS_PER_SAMPLE,$NUM_COLS_PER_SAMPLE
max_versions = 1
num_apps = 1" > dataspaces.conf 

redirect_flag=""
if [ ! -z ${DSPACES_DEBUG+x} ]; then
    redirect_flag="--output=server.out --error=server.err"
fi

echo "Launching DataSpaces server"
flux submit -N $NUM_NODES --cores=$(( NUM_NODES*16 )) \
    --tasks-per-node=$BROKERS_PER_NODE $redirect_flag dspaces_server $DSPACES_HG_STRING

# Wait for DataSpaces's server to create conf.ds
echo "Waiting on conf.ds to be created"
sleep 1s
while [ ! -f conf.ds ]; do
    sleep 1s
done
# Give the server enough time to write the contents
sleep 3s
echo "Server running!"

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
echo "Preloading samples into DataSpaces"
flux run -N $NUM_NODES --cores=$(( NUM_NODES*32 )) --tasks-per-node=32 python3 $GITHUB_WORKSPACE/tests/integration/dlio_benchmark/dspaces_preloader.py $DLIO_DATA_DIR/train

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
if [ $# -ge 1 ]; then
    cd $curr_dir
fi
