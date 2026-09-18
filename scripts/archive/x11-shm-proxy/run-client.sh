#!/bin/bash -e

LD_LIBRARY_PATH=$(pwd)/client:/home/adil/Projects/gramine-tdx-testing/sharedgl/build:$LD_LIBRARY_PATH ./$1
