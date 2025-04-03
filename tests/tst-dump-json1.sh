#!/bin/sh

set -e

INPUT_DIR=../tests/tst-dump-json1.data/input
OUTPUT_DIR=../tests/tst-dump-json1.data/output
EXPECTED_DIR=../tests/tst-dump-json1.data/expected

if [ -d ${OUTPUT_DIR} ]; then
    rm -rf ${OUTPUT_DIR}
fi
mkdir ${OUTPUT_DIR}

for json in k3s-1.31.5+k3s1-29.1.x86-64.raw.json strace-29.1.x86-64.raw.json sysext-deps.json
do
    ./sysext-cli dump-json "$INPUT_DIR/$json" > "$OUTPUT_DIR/$json.out"
    cmp "${OUTPUT_DIR}/$json.out" "${EXPECTED_DIR}/$json.out"
done
