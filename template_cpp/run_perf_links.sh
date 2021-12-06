#!/bin/bash
ID="$1"
./run.sh --id "$ID" --hosts ../example/hosts --output ../example/output/$ID.output ../example/configs/lcausal-broadcast.config
