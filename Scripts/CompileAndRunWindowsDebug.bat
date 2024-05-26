@echo off
if not exist ../Binaries/ (
    echo Binary directory does not exist, running setup script!
    ./SetupWindowsDebug.bat
)

SET BUILD_DIR="../Binaries/"
SET EXE_DIR="../Binaries/App/"

cd /d %BUILD_DIR%

ninja

if %errorlevel%==0 (
    echo Ninja build successful

    %EXE_DIR%App.exe
) else (
    echo Ninja build failed!
)
