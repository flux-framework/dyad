#!/bin/bash

this_script_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd)

source ${this_script_dir}/prod_cons_argparse.sh

mkdir -m 775 -p ${DYAD_PATH_CONSUMER}

if [ -d ${DYAD_INSTALL_PREFIX}/lib64 ] ; then
    DYAD_INSTALL_LIBDIR=${DYAD_INSTALL_PREFIX}/lib64
elif [ -d ${DYAD_INSTALL_PREFIX}/lib ] ; then
    DYAD_INSTALL_LIBDIR=${DYAD_INSTALL_PREFIX}/lib
else
    echo "Cannot find DYAD LIB DIR"
    exit 1
fi

if [[ "$mode" == "${valid_modes[0]}" ]]; then
    LD_PRELOAD=${DYAD_INSTALL_LIBDIR}/libdyad_wrapper.so ${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/c_cons 10 ${DYAD_PATH_CONSUMER}
elif [[ "$mode" == "${valid_modes[1]}" ]]; then
    ${GITHUB_WORKSPACE}/docs/demos/ecp_feb_2023/cpp_cons 10 ${DYAD_PATH_CONSUMER}
elif [[ "$mode" == "${valid_modes[2]}" ]]; then
    python3 ${GITHUB_WORKSPACE}/tests/pydyad_spsc/consumer.py ${DYAD_PATH_CONSUMER} 10 50
else
    echo "Invalid cons test mode: $mode"
    exit 1
fi

ls -l ${DYAD_PATH_CONSUMER}
