#!/bin/bash
/home/roborobo/projects/gauntlet_repo/gauntlet/modules/toz3/pruner/tests/broken_p4c/24895c1b28a35352f1d9ad1f43878c4f7061d3ab -I /home/roborobo/projects/gauntlet_repo/gauntlet/modules/toz3/pruner/example_programs/fabric_with_creduce/ ./fabric.p4 2>&1 > /dev/null | grep "Compiler Bug"
