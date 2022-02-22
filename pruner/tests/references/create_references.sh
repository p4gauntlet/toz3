#!/bin/bash

for f in crash_bugs/*.p4; do
    if [[ "$f" != *"reference.p4" ]]; then
        bf=$(basename "$f" .p4)
        ../../../../p4c/build/extensions/toz3/pruner/p4pruner --seed 1  --compiler-bin ../broken_p4c/24895c1b28a35352f1d9ad1f43878c4f7061d3ab  $f  --bug-type CRASH --output crash_bugs/${bf}_reference.p4
    fi
done
rm -rf crash_bugs/pruned/ crash_bugs/*stripped*
