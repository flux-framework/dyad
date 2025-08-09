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

if [ $# -ne 2 ] ; then
  echo "Usage> $0 exp_dir c|p"
  echo "Shows the timespan of consumers or producers"
fi

data=$1
outdir=tspan
mkdir -p ${outdir}

if [ "$2" == "c" ] ; then
  ff=`ls -1 $data/runs-cons/time.*.txt | sort --version-sort`
  ofname="${outdir}/consumer_span.${data}.txt"
elif [ "$2" == "p" ] ; then
  ff=`ls -1 $data/runs-prod/time.*.txt | sort --version-sort`
  ofname="${outdir}/producer_span.${data}.txt"
fi

if [ "${show}" == "1" ] ; then
  for f in $ff
  do
    echo $f
    cat $f
  done
fi


awk 'BEGIN {
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
  if (FNR == 1) {
    fcnt ++;
    t_start[fcnt] = t;
  }
  else if (FNR == 2) {
    t_end[fcnt] = t;
  }
}
END {
  printf("\nmin = %f\tmax = %f\tspan = %f\n", t_min, t_max, t_max-t_min);
  printf("start");
  for (i=1; i <= fcnt; i++) {
    printf("\t%f", t_start[i] - t_min);
  }
  printf("\n");
  printf("duration");
  for (i=1; i <= fcnt; i++) {
    printf("\t%f", t_end[i] - t_start[i]);
  }
  printf("\n");
  printf("end");
  for (i=1; i <= fcnt; i++) {
    printf("\t%f", t_end[i] - t_min);
  }
  printf("\n");
}' $ff > ${ofname}
