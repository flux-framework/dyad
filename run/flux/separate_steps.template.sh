#!/bin/bash

SPECROOT=`pwd`
JOBSPACE=out-$1
export NO_MERLIN=1
ext_barrier='@EXT_BARRIER@'

LAUNCHER="flux mini submit -N 1 -n 1 -c @CORES_PER_TASK@"

SYNC=0
ITER=@ITER@
KVS_NS=prod-cons
DYAD_INSTALL_PATH=@DYAD_INSTALL_PATH@
DYAD_KVS_DEPTH=2
DYAD_KVS_BINS=256
#SHARED_FS='--shared_fs'
FIXED_FILE_SIZE='--fixed_fsize @NUM_FILES_PER_ITER@ 1048576'
SLEEP='--usec 100000'

# STARTUP
WORKSPACE=${JOBSPACE}/startup
mkdir -p ${WORKSPACE}
export DYAD_ARGS_PARSER_PATH=${SPECROOT}/user_apps/args.sh
${SPECROOT}/dyad/dyad_startup.sh \
                 --workspace ${WORKSPACE} --specroot ${SPECROOT} \
                 --sync ${SYNC} ${SHARED_FS} \
                 --mpath ${DYAD_INSTALL_PATH}/src/modules/dyad.so \
                 --verbose >& ${WORKSPACE}/start.out

flux kvs namespace create ${KVS_NS}

if [  ! -z ${FLUX_PMI_LIBRARY_PATH+x} ]; then
  FPMI2LIB=`dirname ${FLUX_PMI_LIBRARY_PATH}`/libpmi2.so
  if [ -e ${FPMI2LIB} ]; then
    if [  ! -z ${LD_PRELOAD+x} ]; then
      export LD_PRELOAD=${LD_PRELOAD}:${FPMI2LIB}
    else
      export LD_PRELOAD=${FPMI2LIB}
    fi
  fi
fi

############## START STEPS ###############
date +"%T.%3N" > ${JOBSPACE}/time.q.txt

for CONTEXT in `seq 1 @NUM_TASKS_PER_TYPE@`
do
    # PRODUCER
    #hostname >> ${WORKSPACE}/hostname.${CONTEXT}.txt
    WORKSPACE=${JOBSPACE}/runs-prod
    mkdir -p ${WORKSPACE}
    export DYAD_ARGS_PARSER_PATH=${SPECROOT}/user_apps/args.sh
    ${LAUNCHER} ${SPECROOT}/user_apps/step_prod${ext_barrier}.sh \
                --workspace ${WORKSPACE} --specroot ${SPECROOT} \
                --wpath ${DYAD_INSTALL_PATH}/src/wrapper/libdyad_sync.so \
                --kvs_namespace ${KVS_NS} --context ${CONTEXT} \
                --depth ${DYAD_KVS_DEPTH} --bins ${DYAD_KVS_BINS} \
                --sync ${SYNC} ${SHARED_FS} --iter ${ITER} \
                --sync_start @TOTAL_TASKS@ \
                ${SLEEP} ${FIXED_FILE_SIZE} \
                --verbose >& ${WORKSPACE}/runs.${CONTEXT}.out

    # CONSUMER
    #hostname >> ${WORKSPACE}/hostname.${CONTEXT}.txt
    WORKSPACE=${JOBSPACE}/runs-cons
    mkdir -p ${WORKSPACE}
    export DYAD_ARGS_PARSER_PATH=${SPECROOT}/user_apps/args.sh
    ${LAUNCHER} ${SPECROOT}/user_apps/step_cons${ext_barrier}.sh \
                --workspace ${WORKSPACE} --specroot ${SPECROOT} \
                --wpath ${DYAD_INSTALL_PATH}/src/wrapper/libdyad_sync.so \
                --kvs_namespace ${KVS_NS} --context ${CONTEXT} \
                --depth ${DYAD_KVS_DEPTH} --bins ${DYAD_KVS_BINS} \
                --sync ${SYNC} ${SHARED_FS} --iter ${ITER} \
                --sync_start @TOTAL_TASKS@ \
                ${SLEEP} ${FIXED_FILE_SIZE} \
                --verbose --check >& ${WORKSPACE}/runs.${CONTEXT}.out
done
############## END OF STEPS ###############

flux jobs -a
flux queue drain
date +"%T.%3N" >> ${JOBSPACE}/time.q.txt
#flux kvs dir -R >& ${WORKSPACE}/flux_kvs_dir.out
#flux kvs namespace list > ${WORKSPACE}/flux_kvs_namespace_list.txt
flux kvs namespace remove ${KVS_NS}
#flux kvs namespace list > ${WORKSPACE}/flux_kvs_namespace_list_cleanup.txt
#flux hwloc info > ${WORKSPACE}/hwlocinfo.txt
