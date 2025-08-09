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
  echo "Shows the hostnames of consumers or producers"
fi

data=$1
outdir=hostname
mkdir -p ${outdir}

if [ "$2" == "c" ] ; then
  ff=`ls -1 $data/runs-cons/hostname.*.txt | sort --version-sort`
  ofname="${outdir}/consumer_hostname.${data}.txt"
elif [ "$2" == "p" ] ; then
  ff=`ls -1 $data/runs-prod/hostname.*.txt | sort --version-sort`
  ofname="${outdir}/producer_hostname.${data}.txt"
fi

if [ "${show}" == "1" ] ; then
  for f in $ff
  do
    echo $f
    cat $f
  done
fi


awk '{
  id ++;
  hostname[id] = $1;
}
END {
  printf("hostname");
  for (i=1; i <= id; i++) {
    printf(" %s", hostname[i]);
  }
  printf("\n");
}' $ff > ${ofname}
