#!/bin/bash
# FLUX: -N 2
# FLUX: --output=pydyad_spsc_test.out
# FLUX: --error=pydyad_spsc_test.err

if [ -z "${DYAD_INSTALL_LIBDIR}" ]; then
    echo "DYAD_INSTALL_LIBDIR must be defined"
    exit 1
fi
if [ ! -d "${DYAD_INSTALL_LIBDIR}" ]; then
    echo "DYAD_INSTALL_LIBDIR ($DYAD_INSTALL_LIBDIR) does not exist"
    exit 1
fi
if [ ! -f "${DYAD_INSTALL_LIBDIR}/dyad.so" ]; then
    echo "Invalid contents in DYAD_INSTALL_LIBDIR ($DYAD_INSTALL_LIBDIR)"
    exit 1
fi
if [ -z "${DYAD_PATH_CONSUMER}" ]; then
    if [ -z "${DYAD_PATH}" ]; then
        echo "Either DYAD_PATH_CONSUMER or DYAD_PATH must be defined"
        exit 1
    else
        DYAD_PATH_CONSUMER="${DYAD_PATH}"
    fi
fi
if [ -z "${DYAD_PATH_PRODUCER}" ]; then
    if [ -z "${DYAD_PATH}" ]; then
        echo "Either DYAD_PATH_PRODUCER or DYAD_PATH must be defined"
        exit 1
    else
        DYAD_PATH_PRODUCER="${DYAD_PATH}"
    fi
fi
if [ -z "${DYAD_DTL_MODE}" ]; then
    DYAD_DTL_MODE="FLUX_RPC"
fi
if [ -z "${DYAD_KVS_NAMESPACE}" ]; then
    DYAD_KVS_NAMESPACE="pydyad_test"
fi
if [ -z "${NUM_FILES}" ]; then
    NUM_FILES=50
fi
if [ -z "${NUM_INTS}" ]; then
    NUM_INTS=50
fi

export LD_LIBRARY_PATH="$DYAD_INSTALL_LIBDIR:$LD_LIBRARY_PATH"

flux kvs namespace create $KVS_NAMESPACE
flux exec -r all flux module load $DYAD_INSTALL_LIBDIR/dyad.so \
    $DYAD_PATH_PRODUCER $DYAD_DTL_MODE

flux submit -N 1 --tasks-per-node=1 --exclusive \
    --output="pydyad_cons.out" --error="pydyad_cons.err" \
    --env=DYAD_PATH_CONSUMER=$DYAD_PATH_CONSUMER --env=DYAD_DTL_MODE=$DYAD_DTL_MODE --env=DYAD_KVS_NAMESPACE=$KVS_NAMESPACE \
    --flags=waitable \
    python3 consumer.py $DYAD_PATH_CONSUMER $NUM_FILES $NUM_INTS
flux submit -N 1 --tasks-per-node=1 --exclusive \
    --output="pydyad_prod.out" --error="pydyad_prod.err" \
    --env=DYAD_PATH_PRODUCER=$DYAD_PATH_PRODUCER --env=DYAD_DTL_MODE=$DYAD_DTL_MODE --env=DYAD_KVS_NAMESPACE=$KVS_NAMESPACE \
    --flags=waitable \
    python3 producer.py $DYAD_PATH_PRODUCER $NUM_FILES $NUM_INTS
    
flux job wait --all

flux exec -r all flux module remove dyad
flux kvs namespace remove $KVS_NAMESPACE

if [ -d "$DYAD_PATH_CONSUMER" ]; then
    rm -r $DYAD_PATH_CONSUMER
fi
if [ -d "$DYAD_PATH_PRODUCER" ]; then
    rm -r $DYAD_PATH_PRODUCER
fi

if [ $? -ne 0 ]; then
    echo "ERROR: a job crashed with an error"
    exit 1
fi
