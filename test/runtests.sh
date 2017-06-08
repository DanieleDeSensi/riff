#!/bin/bash
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color


for TESTNAME in test1 test2 test3 test4
do
# Ugly, but we need to run the application before the monitor.
    if [[ $TESTNAME != "test4" ]]; then
        sleep 3 && eval ./$TESTNAME 1 &>/dev/null &
    fi
    OUT=$(eval ./$TESTNAME 0 2>&1)
    RESULT=$?
    if [[ $RESULT == 0 ]]; then
        echo -e ${GREEN}"[PASS] "$TESTNAME${NC}
    else
        echo -e ${RED}"[FAIL] "$TESTNAME${NC}; echo "$OUT"; APP=$(pidof ./$TESTNAME | cut -d ' ' -f 2); kill -9 $APP; exit 1
    fi        
done
