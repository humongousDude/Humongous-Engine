#!/bin/bash

# --- Function Definitions ---

# Sets up CMake and rechecks build status.
cmake_setup() {
    echo "Setting up CMake..."
    local script_args="$@"

    cmake -S . -B "$BUILD_DIR" --preset debug
    if [ $? -ne 0 ]; then
        echo "Error: CMake setup failed!"
        exit 1
    fi

    echo "CMake setup successful. Rechecking build status."
    recheck "${script_args}"
}

# Builds the project with Ninja and runs tests.
build() {
    echo "Starting build process..."
    local app_args="$@"

    if [ ! -d "$BUILD_DIR" ]; then
        echo "Error: Build directory '$BUILD_DIR' does not exist!"
        exit 1
    fi

    # Use a subshell to avoid permanent directory change
    (
        echo "Changing to directory '$BUILD_DIR'..."
        cd "$BUILD_DIR" || { echo "Error: Could not change to directory '$BUILD_DIR'."; exit 1; }

        echo "Running Ninja build..."
        ninja
        if [ $? -ne 0 ]; then
            echo "Error: Ninja build failed!"
            exit 1
        fi
    )

    if [ $? -ne 0 ]; then
        exit 1 # Exit if the subshell failed
    fi

    echo "Ninja build successful."

    # Use explicit paths for executables
    local test_exe="$APP_EXE_DIR/AppTests"
    local app_exe="$APP_EXE_DIR/App"

    echo "Running AppTests: '$test_exe'"
    "$test_exe"
    if [ $? -ne 0 ]; then
        echo "Error: AppTests failed! Aborting."
        exit 1
    fi

    echo "AppTests successful."
    echo "Running application: '$app_exe' ${app_args}"
    "$app_exe" "${app_args}"
}

# Checks for existing build directories and calls the appropriate function.
recheck() {
    echo "Checking build status..."
    local script_args="$@"
    local BUILD_DIR="./Binaries"
    local APP_EXE_DIR="./Binaries/App"

    if [ -d "./Binaries-Release" ]; then
        echo "Release build directory './Binaries-Release' found."
        BUILD_DIR="./Binaries-Release"
        APP_EXE_DIR="./Binaries-Release/App"
        APP_EXE_DIR="./Binaries-Release/AppTests"
        build "${script_args}"
        return
    fi

    if [ -d "$BUILD_DIR" ]; then
        echo "Default build directory '$BUILD_DIR' found."
        build "${script_args}"
        return
    fi

    echo "No existing build directory found. Setting up CMake."
    cmake_setup "${script_args}"
}

# --- Main Script Execution ---
recheck "$@"

