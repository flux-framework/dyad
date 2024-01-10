#!/bin/bash

script_name="$0"

if test "$#" -ne 1; then
    echo "Invalid number of arguments to $script_name"
    exit 1
fi

mode="$1"
echo "Need mode: $mode"
valid_modes=("c" "cpp" "python")
mode_is_valid=0
for vm in "${valid_modes[@]}"; do
    if [[ $mode_is_valid -eq 1 ]] || [[ "$mode" == "$vm" ]]; then
        mode_is_valid=1
    else
        mode_is_valid=0
    fi
done
echo "Need valid_modes: $mode_is_valid"

if [[ $mode_is_valid -eq 0 ]]; then
    echo "Invalid arg mode: $mode"
    exit 2
fi
