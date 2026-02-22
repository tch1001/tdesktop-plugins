@echo off
:: Generate compile_commands.json for clangd using Ninja + MSVC
:: Run this from the x64 Native Tools Command Prompt for VS 2022
:: OR it will call vcvars64.bat automatically if cl.exe is not in PATH

where cl.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo Setting up MSVC x64 environment...
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)

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
    -Werror=dev -Werror=deprecated --warn-uninitialized ^
    ..

if %errorlevel% neq 0 (
    echo [ERROR] cmake configure failed
    popd
    exit /b 1
)

popd

:: Copy compile_commands.json to root for clangd
copy /y "%BUILD_DIR%\compile_commands.json" "%~dp0compile_commands.json"
echo.
echo compile_commands.json generated at project root.
echo Clangd is now configured.
