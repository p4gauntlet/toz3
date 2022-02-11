#!/bin/bash
CUR_DIR=`pwd`
${CUR_DIR}/../../tests/broken_p4c/24895c1b28a35352f1d9ad1f43878c4f7061d3ab -I $CUR_DIR ./fabric.p4 2>&1 > /dev/null | grep "Compiler Bug"
