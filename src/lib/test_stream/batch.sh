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

DYAD_PATH=/tmp/yeom2/dyad

./startup.sh --dpath ${DYAD_PATH} --bins 64 --depth 1
#DYAD_OPT="DYAD_KEY_DEPTH=${DYAD_DEPTH} DYAD_KEY_BINS=${DYAD_BINS}"
flux resource list

export DYAD_SYNC_DEBUG=1

flux kvs namespace create TEST1
flux mini submit -N 1 -n 1 -c 56 --output=flux-{{id}}.out bash -c "FLUX_KVS_NAMESPACE=TEST1 ./test_stream ${DYAD_PATH} ${DYAD_PATH}/test.txt 0 0"
flux mini submit -N 1 -n 1 -c 56 --output=flux-{{id}}.out bash -c "FLUX_KVS_NAMESPACE=TEST1 ./test_stream ${DYAD_PATH} ${DYAD_PATH}/test.txt 1 0"

flux kvs namespace create TEST2
flux mini submit -N 1 -n 1 -c 56 --output=flux-{{id}}.out bash -c "FLUX_KVS_NAMESPACE=TEST2 ./test_stream ${DYAD_PATH} ${DYAD_PATH}/test2.txt 0 1"
flux mini submit -N 1 -n 1 -c 56 --output=flux-{{id}}.out bash -c "FLUX_KVS_NAMESPACE=TEST2 ./test_stream ${DYAD_PATH} ${DYAD_PATH}/test2.txt 1 1"

flux jobs -a
flux queue drain

flux kvs namespace remove TEST1
flux kvs namespace remove TEST2
