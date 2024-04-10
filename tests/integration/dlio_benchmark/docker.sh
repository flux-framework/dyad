#!/bin/bash
GENERATE_DATA=$1
source ./setup-env-docker.sh $GENERATE_DATA
flux start --test-size=$NUM_NODES ./run_dlio_docker.sh $GENERATE_DATA
