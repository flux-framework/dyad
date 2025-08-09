#************************************************************
# Copyright 2021 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
#************************************************************

#!/bin/bash


DYAD_MODULE=../../modules/dyad.so
DYAD_PATH=/tmp/${USER}/dyad
DYAD_KVS_NS="test"
RESET_KVS=

# Clean up the dyad directory
pushd `dirname ${DYAD_PATH}` > /dev/null
rm -rf ${DYAD_PATH} 2> /dev/null
mkdir -p ${DYAD_PATH} 2> /dev/null
popd > /dev/null
flux exec -r all rm -rf ${DYAD_PATH}
flux exec -r all mkdir -p ${DYAD_PATH}
flux exec -r all chmod 775 ${DYAD_PATH}

if [ ! -z ${RESET_KVS+x} ] && [ "${RESET_KVS}" != "" ] && [ "${RESET_KVS}" != "0" ] ; then
    kvs_exists=`flux exec -r all flux module list | grep kvs`
    if [ "${kvs_exists}" != "" ] ; then
        flux exec -r all flux module remove kvs
    fi
    flux exec -r all flux module load kvs
fi

# Set KVS namespace for file sharing context
# Remove any quest KVS_NAMESPACE and use the primary namespace to allow the
# producers and the consumers of different jobs to see the same KVS namespace
if [ "${DYAD_KVS_NS}" != "" ] ; then
    if [ "`flux kvs namespace list | grep ${DYAD_KVS_NS}`" != "" ] ; then
        # flush out any existing key-value entries
        flux kvs namespace remove ${DYAD_KVS_NS}
    fi
    flux kvs namespace create ${DYAD_KVS_NS}
    flux kvs namespace list
fi

dyad_exists=`flux exec -r all flux module list | grep dyad`
if [ "${dyad_exists}" != "" ] ; then
    flux exec -r all flux module remove dyad
fi
flux exec -r all flux module load ${DYAD_MODULE} ${DYAD_PATH}
