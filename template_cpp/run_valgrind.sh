#!/bin/bash
ID="$1"
valgrind \
    --suppressions=./valgrind.supp \
    --leak-check=full \
    --show-leak-kinds=all \
    ./bin/da_proc --id "$ID" --hosts ../example/hosts --output ../example/output/$ID.output ../example/configs/perfect-links.config
