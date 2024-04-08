#!/bin/bash

echo "## Config file for DataSpaces server
ndim = 1
dims = ${DSPACES_DEFAULT_VAR_SIZE}
max_versions = ${DSPACES_MAX_VAR_VERSIONS}
num_apps = 1" > dataspaces.conf

dspaces_num_nodes=$(flux resource info | grep -oP "\d+ Nodes" | grep -oP "^\d+")

flux submit -N ${dspaces_num_nodes} --tasks-per-node ${DSPACES_PPN} ${DSPACES_ROOT}/bin/dspaces_server ${DSPACES_HG_CONNECTION_STR}