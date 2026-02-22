$env:QT = "5.15.17"
$vsPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

# Source vcvars env
$envOutput = & cmd /c "`"$vsPath`" > nul 2>&1 && set" 2>&1
foreach ($line in $envOutput) {
    if ($line -match "^([^=]+)=(.*)$") {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
    }
}
$env:QT = "5.15.17"

# Increase compiler heap size to avoid C1060 errors
# /Zm sets the compiler memory allocation limit (multiplier of default)
$env:_CL_ = "/Zm200"

$buildDir = "C:\Users\tanch\Documents\tdesktop\out-clangd"
Set-Location $buildDir

# Reduce parallel jobs to avoid compiler heap space issues
# Use half the CPU count, but at least 2 and at most 8
$maxJobs = [Math]::Max(2, [Math]::Min(8, [Math]::Floor([Environment]::ProcessorCount / 2)))
$jobs = $maxJobs
Write-Host "Building with $jobs parallel jobs (reduced to avoid heap issues)..."

$startTime = Get-Date
# Use Release mode to reduce memory usage during compilation
& cmake --build . --target Telegram --config Release -j $jobs 2>&1
$exitCode = $LASTEXITCODE
$elapsed = (Get-Date) - $startTime

Write-Host ""
Write-Host "Build exit code: $exitCode"
Write-Host "Elapsed: $($elapsed.ToString('hh\:mm\:ss'))"

if ($exitCode -eq 0) {
    Write-Host "Build succeeded!"
    $exePath = "$buildDir\Release\Telegram.exe"
    if (Test-Path $exePath) {
        Write-Host "Binary: $exePath"
    } else {
        Write-Host "Binary location: $buildDir\Release\Telegram.exe (or Debug if Release not found)"
    }
} else {
    Write-Host "Build FAILED"
    exit $exitCode
}
