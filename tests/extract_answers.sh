#!/usr/bin/env bash

EXEC=../src/sched
TAIL=/opt/homebrew/bin/gtail

PATTERN="===Results==="

for testfile in test-*.txt
do
    $EXEC < $testfile > $testfile.log
    nl=$(grep -n $PATTERN < $testfile.log | cut -f1 -d':')
    if [ -z "$nl" ]; then
        echo "*Error: could not fid patern \"$PATTERN\" in $testfile.log"
        exit 1
    fi
    $TAIL -n +$nl < $testfile.log > $testfile.res
done

