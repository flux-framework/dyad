#!/bin/bash

source ./dspaces-setup-env.sh

# export DSPACES_DEBUG=1

if [ $# -eq 0 ]; then
    flux batch -N $NUM_NODES -t $TIME -q $QUEUE --exclusive \
        --broker-opts=--setattr=log-filename=./logs/flux.log \
        ./dspaces_run_dlio.sh
else
    dump_dir=$1
    if [ ! -d $1 ]; then
        mkdir -p $1
    fi
    if [ $# -eq 1 ]; then
        flux batch -N $NUM_NODES -t $TIME -q $QUEUE --exclusive \
            --broker-opts=--setattr=log-filename=./logs/flux.log \
            --output=$1/run.out --error=$1/run.err \
            ./dspaces_run_dlio.sh $1
    else
        export NUM_NODES=$2
        flux batch -N $NUM_NODES -t $TIME -q $QUEUE --exclusive \
            --broker-opts=--setattr=log-filename=./logs/flux.log \
            --output=$1/run.out --error=$1/run.err \
            ./dspaces_run_dlio.sh $1 $2
    fi
fi