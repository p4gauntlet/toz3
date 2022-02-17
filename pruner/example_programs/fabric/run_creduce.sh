#!/bin/bash

if test "$#" -ne 2; then
    echo "Usage run_creduce.sh <filename> <interestingness test>"
    exit 1
fi

FILENAME=$1
INT_TEST=$2
cp $FILENAME ${FILENAME}_creduce_source 
creduce --not-c --tidy $2 $1
mv $FILENAME ${FILENAME}_creduce_result
mv ${FILENAME}_creduce_source ${FILENAME} # Restore the original p4 source