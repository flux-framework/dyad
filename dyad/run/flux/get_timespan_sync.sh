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
  exit
fi

data=$1
outdir=tspan_sync
mkdir -p ${outdir}

if [ "$2" == "c" ] ; then
  ff=`ls -1 $data/runs-cons/runs_cons.*.out | sort --version-sort`
  ofname="${outdir}/consumer_span.${data}.txt"
  is_cons=1
elif [ "$2" == "p" ] ; then
  ff=`ls -1 $data/runs-prod/runs_prod.*.out | sort --version-sort`
  ofname="${outdir}/producer_span.${data}.txt"
  is_cons=0
fi

if [ "${show}" == "1" ] ; then
  for f in $ff
  do
    echo $f
    cat $f
  done
fi


awk -v is_cons=${is_cons} 'BEGIN {
  FS = "[ :]";
}
(NR==1) {
  t_min = 1000000;
  t_max = 0;
}
($1 == "DYAD")
{
  if ((is_cons == 0) && ($2 != "stops")) {
    next
  }
  if ((is_cons == 1) && ($2 != "synchronized")) {
    next
  }
  h = $(NF-2);
  m = $(NF-1);
  s = $(NF);
  t = h*60*60 + m*60 + s;
  if (t < t_min) {
    if (is_cons == 1) {
      d = $(NF-3);
    }
    t_min = t;
  }
  if (t > t_max) {
    if (is_cons == 0) {
      d = $(NF-3);
    }
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
  printf("\nday = %s\tmin = %.9f\tmax = %.9f\n", d, t_min, t_max);
}' $ff > ${ofname}
