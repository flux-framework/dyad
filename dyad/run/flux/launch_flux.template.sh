#!/bin/bash


#flux_start_opts="-o,-S,log-filename=flux_log-separate_steps.out"
#flux_start_opts="-o,--k-ary=@NODES@"
#flux_start_opts="-o,--k-ary=@NODES@,-Srundir=."
flux_start_opts="-o,--k-ary=@NODES@,-S,log-filename=flux_log-separate_steps.out"

if [ "$(echo ${SYS_TYPE} | grep toss)" != "" ] ; then
  srun --pty --mpi=none --mpibind=off -N @NODES@ -n @NODES@ flux start ${flux_start_opts} $1
elif [ "$(echo ${SYS_TYPE} | grep blueos)" != "" ] ; then
  PMIX_MCA_gds="^ds12,ds21" jsrun -a 1 -c ALL_CPUS -g ALL_GPUS -n @NODES@ --bind=none --smpiargs="-disable_gpu_hooks" flux start ${flux_start_opts} $1
fi
