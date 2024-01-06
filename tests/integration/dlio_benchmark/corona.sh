#!/bin/bash
source ./setup-env.sh
flux alloc -N $NUM_NODES --broker-opts=--setattr=log-filename=flux.log --exclusive ./run_dlio.sh
