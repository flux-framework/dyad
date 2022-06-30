flux resource list

@JOB_NODES@
num_of_jobs=10
num_events_per_file=12


# set variables
check_interval=10

spec=separate_steps.${nodes}.sh

rm -rf 0 1 2 3 content.sqlite

sleep ${check_interval}
# first run
bash ${spec} 1

sleep ${check_interval}

# more runs
if [ ${num_of_jobs} -gt 1 ] ; then
    for i in `seq 2 ${num_of_jobs} `
    do
        rm -rf 0 1 2 3 content.sqlite
        bash ${spec} $i

        sleep ${check_interval}

        # wait until the run completes
        sleep ${check_interval}
    done
fi
