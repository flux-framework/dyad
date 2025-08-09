#************************************************************
# Copyright 2021 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
#************************************************************

# Convert a relative path to an absolute path.
# The target path mut be accessible for this function to work.

function absolute_path {
    if [ $# -ne 1 ] ; then
        echo "function ${0}() requires one arguments:" 1>&2
        echo "  1. path" 1>&2
        exit 1
    fi

    local ap_rel_path="$1"
    local ap_bname="$(basename "${ap_rel_path}")"

    if [ "${ap_rel_path}" == "." ]; then
        echo "$(pwd)"
    elif [ "${ap_rel_path}" == ".." ]; then
        echo "$(dirname "$(pwd)")"
    else
        if [ -z ${ap_rel_path} ] || [ ! -d ${ap_rel_path} ] ; then
            echo "${ap_rel_path} does not exist!" 1>&2
            exit 1
        else
            local ap_cwd="$(pushd "${ap_rel_path}" > /dev/null; pwd)"
            case ${ap_bname} in
                ..) echo "$(absolute_path "${ap_cwd}")";;
                 .) echo "${ap_cwd}";;
                 *) echo "${ap_cwd}";;
            esac
        fi
    fi
}
