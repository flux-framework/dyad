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
ff=`ls -1 $ws/runs-cons/time.*.txt $ws/runs-prod/time.*.txt`

awk -v ws=$ws 'BEGIN {
  FS = ":";
}
(NR==1) {
  t_min = 1000000;
  t_max = 0;
}
{
  h = $1;
  m = $2;
  s = $3;
  t = h*60*60 + m*60 + s;
  if (t < t_min) {
    t_min = t;
  }
  if (t > t_max) {
    t_max = t;
  }
}
END {
# printf("%s %.3f %.3f %.3f\n", ws, t_max-t_min, t_min, t_max);
  printf("%s %.3f sec\n", ws, t_max-t_min);
}' $ff

if [ $# -eq 1 ] ; then
  exit
fi

pushd $ws > /dev/null

ff=`ls -1d out-* | sort -V`
for f in $ff
do
  ../get_timespan.sh $f
  ../get_hostname.sh $f
done

tar zcvf tspan.tgz tspan
tar zcvf hostname.tgz hostname

popd > /dev/null


if [ "${show}" == "1" ] ; then
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
