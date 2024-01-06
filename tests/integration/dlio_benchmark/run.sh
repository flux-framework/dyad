#!/bin/bash
source ./setup-env.sh 

# Setup DYAD
echo Setting up Dyad
flux kvs namespace create ${DYAD_KVS_NAMESPACE}
for (( c=0; c<NUM_NODES; c++ )); do     
  echo "Loading module on broker $c"  
  flux exec -r $c flux module load ${SPACK_ENV}/.spack-env/view/lib/dyad.so  $DYAD_PATH UCX
done
echo Loaded Modules
flux module list
echo List Namespaces
flux kvs namespace list
# Setup Local Directories
echo Setting up local directories
flux run -N ${NUM_NODES} --tasks-per-node=1 mkdir -p $DYAD_PATH
flux run -N ${NUM_NODES} --tasks-per-node=1 rm -rf $DYAD_PATH/*

if [[ "${GENERATE_DATA}" == "1" ]]; then
# Generate Data for Workload
echo Generating DLIO Dataset
flux submit -N ${NUM_NODES} --tasks-per-node=${PPN} dlio_benchmark ${CONFIG_ARG} workload=${DLIO_WORKLOAD} ++workload.dataset.data_folder=${DLIO_DATA_DIR} ++workload.workflow.generate_data=True ++workload.workflow.train=False
GEN_PID=$(flux job last)
flux job attach ${GEN_PID}
fi

# Run Training
echo Running DLIO Training for ${DLIO_WORKLOAD}
flux submit -N ${NUM_NODES} -o cpu-affinity=on --tasks-per-node=${PPN} dlio_benchmark ${CONFIG_ARG} workload=${DLIO_WORKLOAD} ++workload.dataset.data_folder=${DLIO_DATA_DIR} ++workload.workflow.generate_data=False ++workload.workflow.train=True
RUN_PID=$(flux job last)
flux job attach ${RUN_PID} > ${DYAD_DLIO_RUN_LOG} 2>&1
#cat ${DYAD_DLIO_RUN_LOG}
echo "Finished Executing check ${DYAD_DLIO_RUN_LOG} for output"


# Clean DYAD
echo Cleaning DYAD
flux kvs namespace remove ${DYAD_KVS_NAMESPACE}
for (( c=0; c<NUM_NODES; c++ )); do     
  flux exec -r $c flux module unload ${SPACK_ENV}/.spack-env/view/lib/dyad.so 
done
