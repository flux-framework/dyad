#************************************************************
# Copyright 2021 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
#************************************************************

#!/bin/bash
ff=`ls -1d *step*_* | sed 's/\x1b\[[0-9;]*m//g'`; for f in $ff; do ../get_results.sh $f; done > result.txt

awk '(NF==3){
    if ($2 ~ /[0-9\.]+/) {
        sum += $2;
        cnt++;
        if ($2 > max) {
            max = $2;
        }
    }
}
END {
    if ((cnt > 1) && (omit_one_outlier != 0)) {
        sum = sum - max;
        printf("%f\n", sum/(cnt-1));
    } else {
        printf("%f\n", sum/cnt);
    }
}' result.txt
