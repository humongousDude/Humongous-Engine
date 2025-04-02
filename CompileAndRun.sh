#!/bin/bash

BUILD_DIR="./Binaries"
EXE_DIR="./Binaries/App"

recheck() {
    if [ -d "$BUILD_DIR" ]; then
        build
        return
    fi

    if [ -d "./Binaries-Release/" ]; then
        BUILD_DIR="./Binaries-Release"
        EXE_DIR="./Binaries-Release/App"
        build
        return
    fi

    cmake_setup
}

build() {
    if [ ! -d "$BUILD_DIR" ]; then
        echo "Error: Build directory $BUILD_DIR does not exist!"
        exit 1
    fi

    cd "$BUILD_DIR" || exit
    ninja

    if [ $? -eq 0 ]; then
        echo "Ninja build successful"
        ./App/App "$@"
    else
        echo "Ninja build failed!"
    fi
}

cmake_setup() {
    echo "Setting up CMake..."
    cmake -S . -B "$BUILD_DIR" --preset release

    if [ $? -ne 0 ]; then
        echo "CMake setup failed!"
        exit 1
    fi

    recheck
}

# Start the process
recheck
