#!/bin/bash
source ./setup-env.sh
rm *.core flux.log
rm -rf logs/* profiler/*
flux alloc -q $QUEUE -t $TIME -N $NUM_NODES -o per-resource.count=${BROKERS_PER_NODE} --exclusive --broker-opts=--setattr=log-filename=./logs/flux.log ./run_dlio.sh
