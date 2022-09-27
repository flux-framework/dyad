#!/bin/gawk -f

#************************************************************
# Copyright 2021 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
#************************************************************

# required parameters:
#   In case of applying to yaml spec files
#                      -v nodes=(number of nodes)
#                      -v cores_per_task=(number of cores per task)
#                      -v cores_per_node=(number of cores per node)
#                      -v max_iters_per_node=(number of iterations)
#                      -v num_files_per_iter=(number of files used per iteration)
#
#   In case of applying to run.template.qsub
#                      -v nodes=(number of nodes)
#                      -v cores_per_node=(number of cores per node)
#                      -v max_iters_per_node=(number of iterations)
#                      -v num_files_per_iter=(number of files used per iteration)
#                      -v work=(d or s)
#                               depending on which one of dependent_steps or
#                               separate_steps are processed

BEGIN {
}

(NR==1) {
    if (max_iters_per_node == 0) {
        max_iters_per_node = 1;
    }
    cores_per_task = int(cores_per_task) + 0;
    if (cores_per_task != 0) {
        num_tasks_per_node = int(cores_per_node / cores_per_task);
    } else {
        cores_per_task = cores_per_node;
        num_tasks_per_node = 1;
    }
    iter = max_iters_per_node / int(num_tasks_per_node);
    conc = nodes * num_tasks_per_node;
    num_tasks_per_type = int((conc+1) / 2); # half for producer and half for consumer
}

{
    len = 0;
    for (i=1; i <= NF; i++) {
        if ($i ~ /@[[:alnum:]_]+@/) {
            len = length($0) - length($i);
            if ($i ~ /@NODES@/) {
                gsub("@NODES@", nodes, $i);
            } else if ($i ~ /@HALF_NODES@/) {
                gsub("@HALF_NODES@", nodes/2, $i);
            } else if ($i ~ /@CORES_PER_TASK@/) {
                gsub("@CORES_PER_TASK@", cores_per_task, $i);
            } else if ($i ~ /@CONC@/) {
                gsub("@CONC@", conc, $i);
            } else if ($i ~ /@HALF_CONC@/) {
                gsub("@HALF_CONC@", conc/2, $i);
            } else if ($i ~ /@ITER@/) {
                gsub("@ITER@", iter, $i);
            } else if ($i ~ /@MAX_ITERS_PER_NODE@/) {
                gsub("@MAX_ITERS_PER_NODE@", max_iters_per_node, $i)
            } else if ($i ~ /@NUM_FILES_PER_ITER@/) {
                gsub("@NUM_FILES_PER_ITER@", num_files_per_iter, $i);
            } else if ($i ~/@NUM_TASKS_PER_TYPE@/) {
                gsub("@NUM_TASKS_PER_TYPE@", num_tasks_per_type, $i);
            } else if ($i ~ /@WORK@/) {
                gsub("@WORK@", work, $i);
            } else if ($i ~ /@JOB_NODES@/) {
                gsub("@JOB_NODES@", "nodes="nodes, $i);
            }
            len = len + length($i);
        }
    }
    if (len > 0) {
        n = len - length($0);
        for (j=1; j <= n; j++) {
            printf(" ");
        }
        n = 0;
    }
    printf("%s\n", $0);
}
