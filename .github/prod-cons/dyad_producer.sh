#!/bin/bash

this_script_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd)

source $this_script_dir/prod_cons_argparse.sh

mkdir -m 775 -p ${DYAD_PATH_PRODUCER}
echo "Loading DYAD module"

if [ -d ${DYAD_INSTALL_PREFIX}/lib64 ] ; then
    DYAD_INSTALL_LIBDIR=${DYAD_INSTALL_PREFIX}/lib64
elif [ -d ${DYAD_INSTALL_PREFIX}/lib ] ; then
    DYAD_INSTALL_LIBDIR=${DYAD_INSTALL_PREFIX}/lib
else
    echo "Cannot find DYAD LIB DIR"
    exit 1
fi

echo flux module load ${DYAD_INSTALL_LIBDIR}/dyad.so --mode="${DYAD_DTL_MODE}" ${DYAD_PATH_PRODUCER}
flux module load ${DYAD_INSTALL_LIBDIR}/dyad.so --mode="${DYAD_DTL_MODE}" ${DYAD_PATH_PRODUCER}

if [[ "$mode" == "${valid_modes[0]}" ]]; then
    echo ${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/c_prod 10 ${DYAD_PATH_PRODUCER}
    LD_PRELOAD=${DYAD_INSTALL_LIBDIR}/libdyad_wrapper.so ${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/c_prod 10 ${DYAD_PATH_PRODUCER}
elif [[ "$mode" == "${valid_modes[1]}" ]]; then
    echo ${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/cpp_prod 10 ${DYAD_PATH_PRODUCER}
    ${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/cpp_prod 10 ${DYAD_PATH_PRODUCER}
elif [[ "$mode" == "${valid_modes[2]}" ]]; then
    echo python3 ${GITHUB_WORKSPACE}/tests/pydyad_spsc/producer.py ${DYAD_PATH_PRODUCER} 10 50
    python3 ${GITHUB_WORKSPACE}/tests/pydyad_spsc/producer.py ${DYAD_PATH_PRODUCER} 10 50
else
    echo "Invalid prod test mode: $mode"
    exit 1
fi

# If this test were to be repeatable, two cleanup steps are needed
# after consumer finishes
# - flux module remove dyad
# - remove the files produced and consumed
