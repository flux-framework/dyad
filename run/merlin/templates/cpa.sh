#!/bin/bash

TDIFF=/p/lustre2/yeom2/FLUX/flux-dyad/exp-cpa2/tdiff.sh

if [ $# -ne 1 ] ; then
    echo $0 workspace
    exit
fi


pushd $1/runs-cons > /dev/null
  
ff=`ls -1d [0-9]*`

for f in $ff
do
    pushd $f > /dev/null
    echo " "
    #wc -l dyad_cpa*
    ${TDIFF}
    popd > /dev/null
done

popd > /dev/null
