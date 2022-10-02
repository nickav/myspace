#!/bin/bash

set -e # exit on error

project_root="$(cd "$(dirname "$0")" && pwd -P)"
exe_name="myspace"
flags="-std=c++11 -Wno-deprecated-declarations -Wno-int-to-void-pointer-cast -Wno-writable-strings -Wno-dangling-else -Wno-switch -Wno-undefined-internal"
libs="-framework Cocoa"

pushd $project_root
  mkdir -p build

  pushd build
    rm -rf cyan*
    time g++ $flags $libs -D DEBUG ../src/main.cpp -o $exe_name

    rm -rf bin
    ./$exe_name ../data bin
  popd
popd
