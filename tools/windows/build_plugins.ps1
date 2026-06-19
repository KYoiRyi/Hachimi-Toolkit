param(
    [string[]]$Plugins = @("Uma-Proxy", "Packet-Capture"),
    [string]$BuildType = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Architecture = ""
)

$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param(
        [string]$Command,
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE"
    }
}

$pluginRoot = "plugins"
$outputDir = "build\windows-plugins"
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

foreach ($pluginName in $Plugins) {
    $pluginPath = Join-Path $pluginRoot $pluginName
    $cmakeFile = Join-Path $pluginPath "CMakeLists.txt"

    if (!(Test-Path $cmakeFile)) {
        Write-Error "Plugin '$pluginName' does not contain a CMakeLists.txt"
    }

    $buildDir = Join-Path $pluginPath "build-windows"
    if (Test-Path $buildDir) {
        Remove-Item -Recurse -Force $buildDir
    }

    Write-Host "Building Windows plugin: $pluginName"
    $configureArgs = @(
        "-S", $pluginPath,
        "-B", $buildDir,
        "-G", $Generator,
        "-DCMAKE_BUILD_TYPE=$BuildType"
    )

    if ($Architecture) {
        $configureArgs += @("-A", $Architecture)
    } elseif ($Generator -like "Visual Studio*") {
        $configureArgs += @("-A", "x64")
    }

    Invoke-Checked "cmake" $configureArgs
    Invoke-Checked "cmake" @("--build", $buildDir, "--config", $BuildType, "--parallel")

    $dlls = Get-ChildItem -Path $buildDir -Recurse -Filter "*.dll"
    if (!$dlls) {
        throw "No DLL was produced for plugin '$pluginName'"
    }

    $dlls | ForEach-Object {
        Copy-Item -Path $_.FullName -Destination $outputDir -Force
    }
}

Write-Host "All Windows plugins built successfully."
