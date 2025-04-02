@echo off

SET BUILD_DIR="../Binaries/"
SET EXE_DIR="../Binaries/App/"

:recheck

if not exist "../Binaries/" (
    echo Binary directory does not exist, checking release directory!
) else (
    goto build
)

if not exist "../Binaries-Release/" (
    echo neither Binary directories exist! Did you run CMake setup?
    goto cmake
) else(
    BUILD_DIR="../Binaries-Release/"
    EXE_DIR="../Binaries-Release/App/"
    goto build
)

:build
cd /d %BUILD_DIR%


ninja

if %errorlevel%==0 (
    echo Ninja build successful

    %EXE_DIR%App.exe %*

) else (
    echo Ninja build failed!
)
goto exit

:cmake
cd ../
cmake . --preset debug
goto recheck

:exit
