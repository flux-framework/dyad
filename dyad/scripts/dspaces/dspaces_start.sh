#!/bin/bash

echo "## Config file for DataSpaces server
ndim = 1
dims = $1
max_versions = $2
num_apps = 1" > dataspaces.conf

# Use provided number of nodes instead of auto-obtained number
# dspaces_num_nodes=$(flux resource info | grep -oP "\d+ Nodes" | grep -oP "^\d+")

flux submit -N $3 --cores=$(( $3*1 )) --tasks-per-node=$4 dspaces_server $5

# Wait for DataSpaces configuration file to be created.
# If we don't do this, the DataSpaces clients will either crash or hang
sleep 1s
while [ ! -f conf.ds ]; do
    sleep 1s
done
sleep 3s
