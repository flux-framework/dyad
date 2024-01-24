#!/bin/bash
source ./setup-env.sh
rm *.core flux.log
rm -rf logs/* profiler/*
flux alloc -q pdebug -N $NUM_NODES -o per-resource.count=${BROKERS_PER_NODE} --exclusive  --broker-opts=--setattr=log-filename=./logs/flux.log --broker-opts=--setattr=k-ary=$NUM_NODES  ./run_dlio.sh
