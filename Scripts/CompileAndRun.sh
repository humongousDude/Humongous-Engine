#!/bin/bash

BUILD_DIR="../Binaries/"
EXE_DIR="../Binaries/App/"

recheck() {
    if [ ! -d "../Binaries/" ]; then
        echo "Binary directory does not exist, checking release directory!"
    else
        build
        return
    fi

    if [ ! -d "../Binaries-Release/" ]; then
        echo "Neither Binary directories exist! Did you run CMake setup?"
        cmake_setup
        return
    else
        BUILD_DIR="../Binaries-Release/"
        EXE_DIR="../Binaries-Release/App/"
        build
        return
    fi
}

build() {
    cd "$BUILD_DIR" || exit

    ninja

    if [ $? -eq 0 ]; then
        echo "Ninja build successful"

        "$EXE_DIR"/App "$@"
    else
        echo "Ninja build failed!"
    fi
}

cmake_setup() {
    cd ../ || exit
    cmake . --preset debug
    recheck
}

# Start the process
recheck
