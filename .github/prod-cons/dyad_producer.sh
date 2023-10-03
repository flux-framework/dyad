#!/bin/bash

this_script_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd)

source $this_script_dir/prod_cons_argparse.sh

export DYAD_PATH_PRODUCER=${DYAD_PATH}_producer
mkdir -p ${DYAD_PATH_PRODUCER}
echo "Loading DYAD module"

echo flux module load ${DYAD_INSTALL_PREFIX}/lib/dyad.so --dtl_mode=$DYAD_DTL_MODE $DYAD_PATH_PRODUCER
flux module load ${DYAD_INSTALL_PREFIX}/lib/dyad.so --dtl_mode=$DYAD_DTL_MODE $DYAD_PATH_PRODUCER

if [[ "$mode" == "${valid_modes[0]}" ]]; then
    echo ${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/c_prod 10 $DYAD_PATH_PRODUCER
    LD_PRELOAD=${DYAD_INSTALL_PREFIX}/lib/libdyad_wrapper.so ${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/c_prod 10 $DYAD_PATH_PRODUCER
elif [[ "$mode" == "${valid_modes[1]}" ]]; then
    echo ${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/cpp_prod 10 $DYAD_PATH_PRODUCER
    ${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/cpp_prod 10 $DYAD_PATH_PRODUCER
elif [[ "$mode" == "${valid_modes[2]}" ]]; then
    echo python3 ${GITHUB_WORKSPACE}/tests/pydyad_spsc/consumer.py $DYAD_PATH_PRODUCER 10 50
    python3 ${GITHUB_WORKSPACE}/tests/pydyad_spsc/consumer.py $DYAD_PATH_PRODUCER 10 50
else
    echo "Invalid prod test mode: $mode"
    exit 1
fi
