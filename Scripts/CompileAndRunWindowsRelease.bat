@echo off
if not exist ../Binaries/ (
    echo Binary directory does not exist, running setup script!
    start "" "./SetupWindowsRelease.bat"

)

SET BUILD_DIR="../Binaries-Release/"
SET EXE_DIR="../Binaries-Release/App/"

cd /d %BUILD_DIR%

ninja

if %errorlevel%==0 (
    echo Ninja build successful

    start "" "%EXE_DIR%App.exe"
) else (
    echo Ninja build failed!
)
