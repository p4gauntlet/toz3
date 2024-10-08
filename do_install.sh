#!/bin/bash

# exit when any command fails
set -e

# fetch submodules and update apt
echo "Initializing submodules..."
git submodule update --init --recursive
sudo apt-get update

SRC_DIR="$(pwd)"

echo "Installing P4C dependencies..."

# Install pip and python
sudo apt install -y python3
sudo apt install -y python3-pip
sudo apt install -y python3-setuptools

# Install the p4 compiler dependencies
sudo apt install -y bison \
                    build-essential \
                    cmake \
                    git \
                    flex \
                    libboost-dev \
                    libboost-graph-dev \
                    libboost-iostreams-dev \
                    libfl-dev \
                    libgc-dev \
                    libgmp-dev \
                    pkg-config

# install python packages using pip
pip3 install --user wheel
pip3 install --user pyroute2 ipaddr ply scapy

# Style checks.
sudo apt install -y clang-tidy

echo "Successfully installed P4C dependencies."
