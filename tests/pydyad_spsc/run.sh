#!/bin/bash
# FLUX: -N 2
# FLUX: --output=pydyad_spsc_test.out
# FLUX: --error=pydyad_spsc_test.err

DYAD_INSTALL_LIBDIR="/g/g90/lumsden1/ws/insitu_benchmark/dyad/_test_install/lib"
KVS_NAMESPACE="pydyad_test"
CONS_MANAGED_PATH="/l/ssd/lumsden1/pydyad_test"
PROD_MANAGED_PATH="/l/ssd/lumsden1/pydyad_test"
NUM_FILES=50
NUM_INTS=500

export LD_LIBRARY_PATH="$DYAD_INSTALL_LIBDIR:$LD_LIBRARY_PATH"

if [ ! -d "$CONS_MANAGED_PATH" ]; then
    mkdir -p $CONS_MANAGED_PATH
fi
if [ ! -d "$PROD_MANAGED_PATH" ]; then
    mkdir -p $PROD_MANAGED_PATH
fi

cat > ./env_file.cons << EOM
DYAD_PATH_CONSUMER=$CONS_MANAGED_PATH
DYAD_KVS_NAMESPACE=$KVS_NAMESPACE
DYAD_DTL_MODE=UCX
EOM

cat > ./env_file.prod << EOM
DYAD_PATH_PRODUCER=$PROD_MANAGED_PATH
DYAD_KVS_NAMESPACE=$KVS_NAMESPACE
DYAD_DTL_MODE=UCX
EOM

flux kvs namespace create $KVS_NAMESPACE
flux exec -r all flux module load $DYAD_INSTALL_LIBDIR/dyad.so \
    $PROD_MANAGED_PATH UCX

flux submit -N 1 --tasks-per-node=1 --exclusive \
    --output="pydyad_cons.out" --error="pydyad_cons.err" \
    --env-file=./env_file.cons \
    --flags=waitable \
    python3 consumer.py $CONS_MANAGED_PATH $NUM_FILES $NUM_INTS
flux submit -N 1 --tasks-per-node=1 --exclusive \
    --output="pydyad_prod.out" --error="pydyad_prod.err" \
    --env-file=./env_file.prod \
    --flags=waitable \
    python3 producer.py $PROD_MANAGED_PATH $NUM_FILES $NUM_INTS
    
flux job wait --all

flux exec -r all flux module unload dyad
flux kvs namespace remove $KVS_NAMESPACE

if [ -d "$CONS_MANAGED_PATH" ]; then
    rm -r $CONS_MANAGED_PATH
fi
if [ -d "$PROD_MANAGED_PATH" ]; then
    rm -r $PROD_MANAGED_PATH
fi

if [ $? -ne 0 ]; then
    echo "ERROR: a job crashed with an error"
    exit 1
fi
