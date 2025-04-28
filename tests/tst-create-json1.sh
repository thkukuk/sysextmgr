#!/bin/sh

set -e

INPUT_DIR=../tests/tst-create-json1.data/input
OUTPUT_DIR=../tests/tst-create-json1.data/output
EXPECTED_DIR=../tests/tst-create-json1.data/expected

if [ -d ${OUTPUT_DIR} ]; then
    rm -rf ${OUTPUT_DIR}
fi
mkdir ${OUTPUT_DIR}

for img in strace-29.1.x86-64 k3s-1.31.5+k3s1-29.1.x86-64;
do
    ./sysextmgrcli create-json -n "$img.raw" -i "$INPUT_DIR/extension-release.$img" -o "$OUTPUT_DIR/$img.raw.json"
    cmp "${OUTPUT_DIR}/$img.raw.json" "${EXPECTED_DIR}/$img.raw.json"
done
