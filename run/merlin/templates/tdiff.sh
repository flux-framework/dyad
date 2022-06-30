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
# Sort the log entries by the timestamp
# Then, computes the elapsed time between consequtive entries

ff=`ls -1 dyad_cpa_* 2> /dev/null`
if [ "$ff" == "" ] ; then
    exit
fi

sort --stable --key=1,2 $ff \
  | sed 's/consumer/CONS/g' \
  | sed 's/producer/PROD/g' \
  | gawk -v CONVFMT=%.17g -v PREC="quad" '
function subfields(s, e) {
   return gensub("(([^,]+,){"s-1"})(([^,]+,){"e-s"}[^,]+).*","\\3","") 
}
{
    t_prev = t_curr;
    cmd = "date --date \""$1" "$2"\" +%s.%N";
    cmd | getline t_curr
    close(cmd)
    if (NR == 1) {
        for (i=1; i<= NF; i++)
            prev[i] = $i
        next;
    }

    delta = t_curr - t_prev;
    printf("%s %s %.9f", prev[1], prev[2], delta)
    for (i=3; i <= NF; i++)
        printf("\t%s", prev[i]);
    printf("\n");
    for (i=1; i<= NF; i++)
        prev[i] = $i
}
END {
    printf("%s %s          0", $1, $2)
    for (i=3; i <= NF; i++)
        printf("\t%s", $i);
    printf("\n");

}'
