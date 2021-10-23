#!/bin/bash
set -e
rm logs/*
./stress.py -r /app/template_cpp/run.sh -t perfect -l logs -p "$1" -m "$2"
wc -l logs/*
