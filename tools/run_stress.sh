#!/bin/bash
set -e
rm -f logs/*
./stress.py -r /app/template_cpp/run.sh -t fifo -l logs -p "$1" -m "$2"
wc -l logs/*
./validate_fifo.py --proc_num "$1" -m 0 $(ls logs/*.output)
