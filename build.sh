#!/bin/bash
set -e # exit on error

# Setup
project_root="$(cd "$(dirname "$0")" && pwd -P)"
exe_name="myspace"
build_hash=$(more "$project_root/.git/refs/heads/master")
publish_path="$project_root..\nickav.github.io"

flags="-std=c++11 -Wno-deprecated-declarations -Wno-int-to-void-pointer-cast -Wno-writable-strings -Wno-dangling-else -Wno-switch -Wno-undefined-internal"
libs=""

# Args
release=0
publish=0
for a in "$@"; do declare $a=1; done
[[ $publish == 1 ]] && release=1

pushd $project_root
  mkdir -p build

  pushd build
    rm -rf cyan*
    time g++ $flags $libs -D DEBUG ../src/main.cpp -o $exe_name

    rm -rf bin

    if [[ $release == 0 ]]; then
      ./$exe_name ../data bin --serve
    fi
    if [[ $release == 1 ]]; then
      ./$exe_name ../data bin --open
    fi
  popd
popd
