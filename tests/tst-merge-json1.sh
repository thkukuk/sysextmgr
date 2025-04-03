#!/bin/sh

set -e

INPUT_DIR=../tests/tst-merge-json1.data/input
OUTPUT_DIR=../tests/tst-merge-json1.data/output
EXPECTED_DIR=../tests/tst-merge-json1.data/expected

if [ -d ${OUTPUT_DIR} ]; then
    rm -rf ${OUTPUT_DIR}
fi
mkdir ${OUTPUT_DIR}

./sysext-cli merge-json -o "$OUTPUT_DIR/sysext-deps.json" "${INPUT_DIR}/k3s-1.31.5+k3s1-29.1.x86-64.raw.json" "${INPUT_DIR}/strace-29.1.x86-64.raw.json"
cmp "${OUTPUT_DIR}/sysext-deps.json" "${EXPECTED_DIR}/sysext-deps.json"
