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


tasks_per_node=1
max_iters_per_node=10
num_files_per_iter=2

gen_dependent="yes"
gen_separate="yes"

. ./absolute_path.sh

#DYAD_INSTALL_PATH="${HOME}/FLUX/flux-dyad-${SYS_TYPE}"
if [ ! -d "${DYAD_INSTALL_PATH}" ] ; then
  echo "Set DYAD_INSTALL_PATH"
  exit
fi

#---------------------------------------------------------------------
CLUSTER=$(hostname | sed 's/\([a-zA-Z][a-zA-Z]*\)[0-9]*/\1/g')

cores_per_node=`cat /proc/cpuinfo | grep processor | awk '{if (mx < $NF) mx = $NF} END {print mx+1}'`

if [ "$(echo ${SYS_TYPE} | grep toss)" != "" ] ; then
  q=qsub
  if [ "${CLUSTER}" = "quartz" ]; then
    #cores_per_node=36
    cores_per_task=$(echo ${cores_per_node} / ${tasks_per_node} | bc)
  elif [ "${CLUSTER}" = "ruby" ]; then
    #cores_per_node=56
    cores_per_task=$(echo ${cores_per_node} / ${tasks_per_node} | bc)
  else
    echo "Unknown cluster ${CLUSTER}"
    exit
  fi
elif [ "$(echo ${SYS_TYPE} | grep blueos)" != "" ] ; then
  q=bsub
  #cores_per_node=40
  tasks_per_rs=${tasks_per_node}
  cores_per_task=$(echo ${cores_per_node} / ${tasks_per_node} | bc)
fi

DYAD_INSTALL_PATH=$(absolute_path $DYAD_INSTALL_PATH)
DYAD_PATH_STRING=$(echo ${DYAD_INSTALL_PATH} | sed -e 's/\//\\\//g')

DYAD_MOD_PATH=$(absolute_path ${DYAD_INSTALL_PATH}/src/merlin/dyad)
DYAD_APP_PATH=$(absolute_path ${DYAD_INSTALL_PATH}/src/merlin/user_apps)

if [ ! -d "${DYAD_INSTALL_PATH}" ] ; then
  echo "DYAD_INSTALL_PATH (${DYAD_INSTALL_PATH}) does not exist!"
  exit
fi

for n in 2 4 8 16 32 64 128
do
  if [ "${gen_dependent}" == "yes" ] ; then
    odir=d${n}-cpt${cores_per_task}
    mkdir -p ${odir}

    ./gen.awk -v nodes=$n \
              -v cores_per_task=${cores_per_task} \
              -v cores_per_node=${cores_per_node} \
              -v max_iters_per_node=${max_iters_per_node} \
              -v num_files_per_iter=${num_files_per_iter} \
              dependent_step.template.yaml \
              | sed -e 's/@NODES@/'$n'/g' \
              > ${odir}/dependent_step.${n}.yaml


    pushd ${odir}
    if [ ! -L dyad ] ; then
      cp -RP ${DYAD_MOD_PATH} .
      pushd $(basename ${DYAD_MOD_PATH}) > /dev/null
      ln -sfn ${DYAD_INSTALL_PATH}/src/modules/dyad.so dyad.so
      ln -sfn ${DYAD_INSTALL_PATH}/src/wrapper/libdyad_sync.so libdyad_sync.so
      popd > /dev/null

      cp -RP ${DYAD_APP_PATH} .
      pushd $(basename ${DYAD_APP_PATH}) > /dev/null
      ln -sfn ${DYAD_INSTALL_PATH}/src/wrapper/test/check_file check_file
      ln -sfn ${DYAD_INSTALL_PATH}/src/wrapper/test/combined_test combined_test
      ln -sfn ${DYAD_INSTALL_PATH}/src/wrapper/test/cons_test cons_test
      ln -sfn ${DYAD_INSTALL_PATH}/src/wrapper/test/prod_test prod_test
      popd > /dev/null
    fi
    popd

    ./gen.awk -v nodes=$n -v work=d \
              -v cores_per_node=${cores_per_node} \
              -v max_iters_per_node=${max_iters_per_node} \
              -v num_files_per_iter=${num_files_per_iter} \
              run.template.${q} \
         | sed -e 's/@NODES@/'$n'/g' \
         | sed -e 's/@TASKS_PER_RS@/'${tasks_per_rs}'/g' \
         > ${odir}/run.${q}
  fi


  if [ "${gen_separate}" = "yes" ] ; then
    odir=s${n}-cpt${cores_per_task}
    mkdir -p ${odir}

    ./gen.awk -v nodes=$n \
              -v cores_per_task=${cores_per_task} \
              -v cores_per_node=${cores_per_node} \
              -v max_iters_per_node=${max_iters_per_node} \
              -v num_files_per_iter=${num_files_per_iter} \
              separate_steps.template.yaml \
              | sed -e 's/@NODES@/'$n'/g' \
              | sed -e 's/@DYAD_INSTALL_PATH@/'${DYAD_PATH_STRING}'/g' \
              > ${odir}/separate_steps.${n}.yaml


    pushd ${odir}
    if [ ! -L dyad ] ; then
      cp -RP ${DYAD_MOD_PATH} .
      pushd $(basename ${DYAD_MOD_PATH}) > /dev/null
      ln -sfn ${DYAD_INSTALL_PATH}/src/modules/dyad.so dyad.so
      ln -sfn ${DYAD_INSTALL_PATH}/src/wrapper/libdyad_sync.so
      popd > /dev/null

      cp -RP ${DYAD_APP_PATH} .
      pushd $(basename ${DYAD_APP_PATH}) > /dev/null
      ln -sfn ${DYAD_INSTALL_PATH}/src/wrapper/test/check_file check_file
      ln -sfn ${DYAD_INSTALL_PATH}/src/wrapper/test/combined_test combined_test
      ln -sfn ${DYAD_INSTALL_PATH}/src/wrapper/test/cons_test cons_test
      ln -sfn ${DYAD_INSTALL_PATH}/src/wrapper/test/prod_test prod_test
      popd > /dev/null
    fi
    popd

    ./gen.awk -v nodes=$n -v work=s \
              -v cores_per_node=${cores_per_node} \
              -v max_iters_per_node=${max_iters_per_node} \
              -v num_files_per_iter=${num_files_per_iter} \
              run.template.${q} \
         | sed -e 's/@NODES@/'$n'/g' \
         | sed -e 's/@TASKS_PER_RS@/'${tasks_per_rs}'/g' \
         > ${odir}/run.${q}
  fi
done
