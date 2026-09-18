#!/bin/bash -e

mkdir -p build
pushd build
cmake ../
cmake --build . --target sglrenderer --target sharedgl-core --config Release
# ./sglrenderer -n -p 6000 -s
# ./sglrenderer -m 64 -v
./sglrenderer -x
./sglrenderer -m 64 -v
popd
