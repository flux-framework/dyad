#!/bin/bash

source ./dspaces-setup-env.sh

# export DSPACES_DEBUG=1

if [ $# -eq 0 ]; then
    flux alloc -N $NUM_NODES -t $TIME -q $QUEUE --exclusive \
        --broker-opts=--setattr=log-filename=./logs/flux.log \
        ./dspaces_run_dlio.sh
else
    flux alloc -N $NUM_NODES -t $TIME -q $QUEUE --exclusive \
        --broker-opts=--setattr=log-filename=./logs/flux.log \
        ./dspaces_run_dlio.sh $1
fi
