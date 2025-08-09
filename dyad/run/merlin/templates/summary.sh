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

if [ $# -lt 1 ] ; then
  echo "$0 workspace"
  exit
fi

ws=$1

cat $ws/time.txt | tr ':' ' ' | awk -v ws=$ws '{
  if (NR == 1) {
    h = $1;
    m = $2;
    s = $3;
  } else if (NR == 2) {
    h = $1 - h;
    m = $2 - m;
    s = $3 - s;
  }
}
END {
  if (NR < 2)
      printf("Not finished yet!\n");
  else
      printf("%s %.3f ", ws, h*60*60 + m*60 + s);
}'


if [ -f $ws/merlin_info/context.csv ] ; then
  echo "------"
  # run with sampling of context
  context=`cat $ws/merlin_info/context.csv 2> /dev/null`

  for ctx in $context
  do
    h_prod=`cat $ws/runs-prod/hostname.${ctx}.txt`
    t_prod=`cat $ws/runs-prod/time.${ctx}.txt | tr '\n' ' '`
    o_prod=`cat $ws/runs-prod/runs_prod.${ctx}.out | awk 'BEGIN { sum = 0.0; }
    {
        if ($1 == "Iter") {
            for (i=3; i<=NF; i++) {
                sum += $i;
            }
        }
    }
    END {
        printf ("%f\n", sum);
    }'`

    h_cons=`cat $ws/runs-cons/hostname.${ctx}.txt`
    t_cons=`cat $ws/runs-cons/time.${ctx}.txt | tr '\n' ' '`
    o_cons=`cat $ws/runs-cons/runs_cons.${ctx}.out | awk 'BEGIN { sum = 0.0; }
    {
        if ($1 == "Iter") {
            for (i=3; i<=NF; i++) {
                sum += $i;
            }
        }
    }
    END {
        printf ("%f\n", sum);
    }'`

    echo "- producer ${ctx}: ${h_prod} ${t_prod} ${o_prod}"
    cat $ws/runs-prod/runs_prod.${ctx}.err 2> /dev/null
    echo "- consumer ${ctx}: ${h_cons} ${t_cons} ${o_cons}"
    cat $ws/runs-cons/runs_cons.${ctx}.err 2> /dev/null
  done
  echo "------"
fi | awk 'BEGIN { \
    prod_sum = 0.0;
    cons_sum = 0.0;
    cnt = 0;
}
{
    if ($2 == "producer") {
        cnt++;
        prod[cnt] = $NF + 0.0;
        prod_sum += $NF;
    } else if ($2 == "consumer") {
        cons[cnt] = $NF + 0.0;
        cons_sum += $NF;
    }
}
END {
    if (cnt == 0) exit;

    prod_avg = prod_sum / cnt;
    cons_avg = cons_sum / cnt;
    for (i=1; i <= cnt; i++) {
        p_diff = prod[i]-prod_avg;
        c_diff = cons[i]-cons_avg;
        prod_dev += p_diff * p_diff;
        cons_dev += c_diff * c_diff;
    }
    prod_std = sqrt(prod_dev/cnt);
    cons_std = sqrt(cons_dev/cnt);
    printf("%f %f %f %f\n", prod_avg, prod_std, cons_avg, cons_std);
}'
