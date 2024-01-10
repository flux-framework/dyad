#!/bin/bash
source ./setup-env.sh
flux alloc -N $NUM_NODES -o per-resource.count=${BROKERS_PER_NODE} --exclusive --broker-opts=--setattr=log-filename=flux.log ./run_dlio.sh