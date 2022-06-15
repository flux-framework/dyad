#!/bin/bash
ff=`ls -1d out-*`; for f in $ff; do ../get_results.sh $f; done > result.txt

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
