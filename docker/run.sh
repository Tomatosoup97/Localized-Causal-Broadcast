#!/bin/bash
set -e

docker run \
    --rm \
    --volume "/home/mu/EPFL/DA/project-repo/template_cpp:/app" \
    -it da_image \
    /bin/bash
