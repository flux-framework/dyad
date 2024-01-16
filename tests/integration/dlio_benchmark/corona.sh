#!/bin/bash
source ./setup-env.sh
rm *.core flux.log
flux alloc -q pdebug -N $NUM_NODES -o per-resource.count=${BROKERS_PER_NODE} --exclusive --broker-opts=--setattr=log-filename=flux.log ./run_dlio.sh
