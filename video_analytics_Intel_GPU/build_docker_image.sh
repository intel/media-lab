#!/bin/bash

tag=$1

if [ -z "$tag" ]; then
    tag=latest
fi

docker build -f Dockerfile -t vaig_r3:$tag \
    --build-arg http_proxy=${HTTP_PROXY} \
    --build-arg https_proxy=${HTTPS_PROXY} \
    .