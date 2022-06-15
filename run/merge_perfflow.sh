#!/usr/bin/bash
#set -x
if [ $# -ne 1 ]
then
    echo "Usage: $0 DIR"
    exit 1
fi
DIR="$1"
echo "["
for fn in "${DIR}"/*.pfw
do
    tail -n +2 "${fn}"
done

