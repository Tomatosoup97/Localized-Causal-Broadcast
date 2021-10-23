#!/bin/bash
set -e

docker run \
    --rm \
    --cap-add=NET_ADMIN \
    --volume "/home/mu/EPFL/DA/project-repo:/app" \
    -it da_image \
    /bin/bash
