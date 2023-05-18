#!/bin/bash
ID=$1
cargo run -- --id $ID --hosts ../example/hosts --output ../example/output/$ID.output --config ../example/configs/lcausal-broadcast.config
