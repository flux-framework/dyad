#!/bin/bash

source ./dspaces-setup-env.sh

flux batch -N $NUM_NODES -t $TIME -q $QUEUE --exclusive \
    --broker-opts=--setattr=log-filename=./logs/flux.log \
    ./dspaces_run_dlio.sh