#!/bin/bash -e

pushd quake2_bin
make
popd

rm -rf /root/.yq2
mkdir /root/.yq2
make clean && make
gramine-vm quake2