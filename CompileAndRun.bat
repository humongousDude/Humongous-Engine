@echo off
setlocal

set "BUILD_DIR=.\Binaries"
set "EXE_DIR=.\Binaries\App"

:recheck
    echo Checking build status...
    set "script_args=%*"


    if exist ".\Binaries-Release\" (
        echo Release build directory ".\Binaries-Release" found.
        set "BUILD_DIR=.\Binaries-Release"
        set "EXE_DIR=.\Binaries-Release\App"
        call :build %script_args%
        goto :EOF
    )

    if exist "%BUILD_DIR%\" (
        echo Default build directory "%BUILD_DIR%" found.
        call :build %script_args%
        goto :EOF
    )

    echo No existing build directory found. Setting up CMake.
    call :cmake_setup %script_args%
    goto :EOF

:build
    echo Starting build process...
    set "app_args=%*"

    if not exist "%BUILD_DIR%\" (
        echo Error: Build directory "%BUILD_DIR%" does not exist!
        exit /b 1
    )

    pushd "%BUILD_DIR%"
    if %ERRORLEVEL% neq 0 (
        echo Error: Could not change to directory "%BUILD_DIR%".
        exit /b 1
    )

    echo Running Ninja build...
    ninja
    if %ERRORLEVEL% equ 0 (
        echo Ninja build successful.
        popd
        echo Running application: "%EXE_DIR%\App.exe" %app_args%
        cd "%EXE_DIR%"
        "App.exe" %app_args%
    ) else (
        echo Ninja build failed!
        popd
        exit /b 1
    )
    goto :EOF

:cmake_setup
    echo Setting up CMake...
    set "script_args=%*"

    cmake -S . -B "%BUILD_DIR%" --preset debug
    if %ERRORLEVEL% neq 0 (
        echo CMake setup failed!
        exit /b 1
    )

    echo CMake setup successful. Rechecking build status.
    call :recheck %script_args%
    goto :EOF

call :recheck %*

endlocal
