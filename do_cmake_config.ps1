$env:QT = "5.15.17"
$vsPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

# Source vcvars via cmd and capture env
$envOutput = & cmd /c "`"$vsPath`" > nul 2>&1 && set" 2>&1
foreach ($line in $envOutput) {
    if ($line -match "^([^=]+)=(.*)$") {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
    }
}

$env:QT = "5.15.17"

$buildDir = "C:\Users\tanch\Documents\tdesktop\out-clangd"
if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }

Set-Location $buildDir

$cmakeArgs = @(
    "-G", "Ninja",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DTDESKTOP_API_ID=28427739",
    "-DTDESKTOP_API_HASH=b7aacec7e9d2af2ede313e9e2908d35f",
    "-DDESKTOP_APP_DISABLE_AUTOUPDATE=ON",
    "-DDESKTOP_APP_DISABLE_CRASH_REPORTS=ON",
    ".."
)

Write-Host "Running cmake..."
& cmake @cmakeArgs
$exitCode = $LASTEXITCODE
Write-Host "cmake exit code: $exitCode"

if ($exitCode -eq 0) {
    Copy-Item "$buildDir\compile_commands.json" "C:\Users\tanch\Documents\tdesktop\compile_commands.json" -Force
    Write-Host "compile_commands.json copied to project root."
} else {
    Write-Host "[ERROR] cmake configure failed"
    exit $exitCode
}
