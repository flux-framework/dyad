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

cat $ws/time.txt | tr ':' ' ' | awk -v ws=$ws \
'{
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
  if (NR < 2) {
      printf("Not finished yet!\n");
  } else {
      printf("%s %.3f sec\n", ws, h*60*60 + m*60 + s);
  }
}'

if [ $# -eq 1 ] ; then
  exit
fi

if [ -f $ws/merlin_info/context.csv ] ; then
  echo "------"
  # run with sampling of context
  context=`cat $ws/merlin_info/context.csv | tr '\n' ' ' 2> /dev/null`

  for ctx in $context
  do
    h_prod=`cat $ws/runs-prod/hostname.${ctx}.txt`
    t_prod=`cat $ws/runs-prod/time.${ctx}.txt | tr '\n' ' '`
#echo "checking $ws/runs-prod/runs_prod.${ctx}.out "
#    cat $ws/runs-prod/runs_prod.${ctx}.out | awk '{if ($1=="Iter") print $3;}'
    o_prod=`cat $ws/runs-prod/runs_prod.${ctx}.out | awk '{
        if ($1=="Iter") {
          for (i=3; i<=NF; i++) {
            sum += $i;
          }
          printf("%f %f %d\n", sum, sum/(NF-2), NF-2);
        }
    }'`

    h_cons=`cat $ws/runs-cons/hostname.${ctx}.txt`
    t_cons=`cat $ws/runs-cons/time.${ctx}.txt | tr '\n' ' '`
    o_cons=`cat $ws/runs-cons/runs_cons.${ctx}.out | awk '{
        if ($1=="Iter") {
          for (i=3; i<=NF; i++) {
            sum += $i;
          }
          printf("%f %f %d\n", sum, sum/(NF-2), NF-2);
        }
    }'`

    echo "- producer ${ctx}: ${h_prod} ${t_prod} ${o_prod}"
    cat $ws/runs-prod/runs_prod.${ctx}.err 2> /dev/null
    echo "- consumer ${ctx}: ${h_cons} ${t_cons} ${o_cons}"
    cat $ws/runs-cons/runs_cons.${ctx}.err 2> /dev/null
  done
  echo "------"
else
  echo "------"
  h_cons=`cat $ws/runs-cons/hostname.txt`
  t_cons=`cat $ws/runs-cons/time.txt`
  h_prod=`cat $ws/runs-prod/hostname.txt`
  t_prod=`cat $ws/runs-prod/time.txt`

  echo "- producer: ${h_prod} ${t_prod}"
  cat $ws/runs-prod/runs_prod.out
  cat $ws/runs-prod/*.err 2> /dev/null
  echo "- consumer: ${h_cons} ${t_cons}"
  cat $ws/runs-cons/runs_cons.out
  cat $ws/runs-cons/*.err 2> /dev/null
  echo "------"
fi
