#!/bin/bash

source tests/regression/common.sh

check_env
setup_build_opts

# Get command line arguments
# Current arguments parsed are <sleep_time> <remaining_args>
get_cli_args $@

# Install prerequisites
make
pip3 install --user -r tests/regression/requirements.txt

# FIXME: Check Redis status
redis-server &

# Iterate over all available build options
for (( i = 0; i < ${#OPTIONS[@]}; i++ )); do
    option=${OPTIONS[${i}]}
    cli_arg=$(echo ${option} | cut -d '|' -f 2)
    build_arg=$(echo ${option} | cut -d '|' -f 1)

    trap 'TA_INIT=1' USR1
    bazel run accelerator ${build_arg} -- --ta_port=${TA_PORT} ${cli_arg} &
    TA=$!
    
    TA_INIT=0
    while [ "$TA_INIT" -ne 1 ]; do
        if ps -p $TA > /dev/null; then
            echo "waiting for tangle-accelerator initialization"
            sleep 1
            continue
        else
            # pid does not exist
            break
        fi
    done
    
    trap "kill -9 ${TA};" INT # Trap SIGINT from Ctrl-C to stop TA

    python3 tests/regression/runner.py ${remaining_args} --url localhost:${TA_PORT}
    rc=$?

    if [ $rc -ne 0 ]
    then
        echo "Build option '${option}' failed"
        fail+=("${option}")
    else
        success+=("${option}")
    fi

    bazel clean
    wait $(kill -9 ${TA})
done

echo "--------- Successful build options ---------"
for (( i = 0; i < ${#success[@]}; i++ )); do echo ${success[${i}]}; done
echo "----------- Failed build options -----------"
for (( i = 0; i < ${#fail[@]}; i++ )); do echo ${fail[${i}]}; done

if [ ${#fail[@]} -gt 0 ]; then
    exit 1
fi
