#!/bin/bash
source ./setup-env-demo.sh
flux start --test-size=$NUM_NODES ./run_dlio_docker.sh
