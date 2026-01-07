#!/bin/bash
set -e

# Run setup in Ubuntu 22.04 Docker container
docker run --rm \
    ubuntu:22.04 \
    bash -c "
        set -e
        apt-get update
        apt-get install -y git python3 python3-pip curl sudo
        git clone https://github.com/q2333gh/Lucid --depth=1
        cd Lucid
        python3 setup.py
    "
