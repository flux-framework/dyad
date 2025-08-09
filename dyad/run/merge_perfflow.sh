#************************************************************
# Copyright 2021 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
#************************************************************

#!/usr/bin/bash
#set -x
if [ $# -ne 1 ]
then
    echo "Usage: $0 DIR"
    exit 1
fi
DIR="$1"
echo "["
for fn in "${DIR}"/*.pfw
do
    tail -n +2 "${fn}"
done

