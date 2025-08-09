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

ff=`ls -1d out-* | sort -V`
for f in $ff
do
  ../get_timespan.sh $f c
  ../get_timespan.sh $f p
  ../get_timespan_sync.sh $f c
  ../get_timespan_sync.sh $f p
done

odir=tspan
odir_sync=tspan_sync

pushd ${odir} > /dev/null
ff=`ls -1 | sort -V | grep consumer | sed 's/consumer//'`
popd > /dev/null

echo "========================"

for f in $ff
do
  #day = 05/17/21	min = 85807.450304	max = 85807.450407
  dname=`echo ${f} | cut -d '.' -f 2`
  gawk -v dname=${dname} '{
      if ($1 == "day") {
          start_cnt ++;
          if (start_cnt == 1) {
              prod_day = $3;
              prod_start = $6;
          } else if (start_cnt == 2) {
              cons_day = $3;
              cons_start = $6;
          } else {
              print "ERROR ", $0
          }
      } else if ((NF == 9) && ($4 == "max")) {
          end_cnt ++;
          if (end_cnt == 1) {
              prod_end = $6 + 25200; # 7-hour to UTC
          } else if (end_cnt == 2) {
              cons_end = $6 + 25200;
          } else {
              print "ERROR ", $0
          }
      }
  }
  END {
      if ((prod_day < cons_day) || (prod_start < cons_start)) {
          t_start = prod_start;
          t_day = prod_day;
      } else {
          t_start = cons_start;
          t_day = cons_day;
      }
      if (prod_end < cons_end) {
          t_end = cons_end;
      } else {
          t_end = prod_end;
      }
      printf("%s\t%s\t%.7f\t%.7f\t%.7f\n", dname, t_day, t_start, t_end, t_end - t_start);
  }' ${odir_sync}/producer${f} ${odir_sync}/consumer${f} \
     ${odir}/producer${f} ${odir}/consumer${f} 
done
