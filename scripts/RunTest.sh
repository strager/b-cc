#!/bin/sh
set -eu
if [ "${#}" -eq 0 ]; then
    cat >&2 <<EOF
usage: B_RUN_TEST_PREFIX=... ${0} command [args ...]
EOF
    exit 1
fi
exec ${B_RUN_TEST_PREFIX:-} "${@}"
