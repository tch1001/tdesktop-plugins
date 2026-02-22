@echo off
set QT=5.15.17
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1

set BUILD_DIR=%~dp0out-clangd
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"

cmake -G Ninja ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DTDESKTOP_API_ID=28427739 ^
    -DTDESKTOP_API_HASH=b7aacec7e9d2af2ede313e9e2908d35f ^
    -DDESKTOP_APP_DISABLE_AUTOUPDATE=ON ^
    -DDESKTOP_APP_DISABLE_CRASH_REPORTS=ON ^
    ..

set CMAKE_ERR=%errorlevel%
popd

if %CMAKE_ERR% neq 0 (
    echo [ERROR] cmake configure failed
    exit /b %CMAKE_ERR%
)

copy /y "%BUILD_DIR%\compile_commands.json" "%~dp0compile_commands.json"
echo Done. compile_commands.json ready.
