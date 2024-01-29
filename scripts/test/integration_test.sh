#!/bin/bash

"$1" & disown
PID=$!
trap 'kill $PID' EXIT

sleep 1
redis-benchmark -t GET,SET || exit 1
redis-cli SET test passed | grep OK || exit 1
redis-cli GET test | grep passed || exit 1
