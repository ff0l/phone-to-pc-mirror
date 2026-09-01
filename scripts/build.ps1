param(
    [ValidateSet("bootstrap", "build", "package")]
    [string]$Action = "package",
    [string]$MsysRoot = "C:\msys64"
)

$ErrorActionPreference = "Stop"
$Repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Bash = Join-Path $MsysRoot "usr\bin\bash.exe"
$Ucrt = Join-Path $MsysRoot "ucrt64"

function Invoke-Msys([string]$Command) {
    if (-not (Test-Path $Bash)) {
        throw "MSYS2 not found at $MsysRoot"
    }
    & $Bash -lc $Command
    if ($LASTEXITCODE -ne 0) {
        throw "msys command failed: $Command"
    }
}

function Convert-ToMsysPath([string]$WinPath) {
    $full = (Resolve-Path $WinPath).Path
    $drive = $full.Substring(0, 1).ToLowerInvariant()
    $rest = $full.Substring(2).Replace("\", "/")
    return "/$drive$rest"
}

function Invoke-Bootstrap {
    Invoke-Msys "pacman -S --noconfirm --needed mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-libplist mingw-w64-ucrt-x86_64-gstreamer mingw-w64-ucrt-x86_64-gst-plugins-base mingw-w64-ucrt-x86_64-gst-libav mingw-w64-ucrt-x86_64-gst-plugins-good mingw-w64-ucrt-x86_64-gst-plugins-bad mingw-w64-ucrt-x86_64-openssl"
}

function Copy-GccRuntime([string]$Dest) {
    foreach ($name in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
        $src = Join-Path $Ucrt "bin\$name"
        if (Test-Path $src) {
            Copy-Item -Force $src (Join-Path $Dest $name)
        }
    }
}

function Invoke-Build {
    $unix = Convert-ToMsysPath $Repo
    Invoke-Msys "export PATH=/ucrt64/bin:`$PATH; cmake -S '$unix' -B '$unix/build' -G Ninja -DCMAKE_BUILD_TYPE=Release -DUXPLAY_AS_LIBRARY=ON && cmake --build '$unix/build'"
    Copy-GccRuntime (Join-Path $Repo "build")
}

function Add-DllDeps([string]$Binary, [string]$DestBin, $Seen) {
    $unixBin = Convert-ToMsysPath $Binary
    $lines = & $Bash -lc "export PATH=/ucrt64/bin:`$PATH; ldd '$unixBin'"
    foreach ($line in $lines) {
        if ($line -notmatch "=>\s+(\S+)") {
            continue
        }
        $raw = $Matches[1]
        $dll = $raw
        if ($raw -match "^/ucrt64/") {
            $dll = Join-Path $MsysRoot ($raw.TrimStart("/").Replace("/", "\"))
        } elseif ($raw -match "^/[a-zA-Z]/") {
            $drive = $raw.Substring(1, 1)
            $dll = ($drive + ":" + $raw.Substring(2)).Replace("/", "\")
        } else {
            $dll = $raw.Replace("/", "\")
        }
        if ($dll -notmatch "ucrt64\\bin\\" -and $dll -notmatch "ucrt64\\lib\\") {
            continue
        }
        $name = Split-Path $dll -Leaf
        if ($Seen.ContainsKey($name)) {
            continue
        }
        $Seen[$name] = $true
        $target = Join-Path $DestBin $name
        if (-not (Test-Path $target)) {
            Copy-Item -Force $dll $target
        }
        Add-DllDeps $dll $DestBin $Seen
    }
}

function Invoke-Package {
    Invoke-Build
    $dist = Join-Path $Repo "dist"
    $pluginSrc = Join-Path $Ucrt "lib\gstreamer-1.0"
    $pluginDst = Join-Path $dist "lib\gstreamer-1.0"
    if (Test-Path $dist) {
        Remove-Item -Recurse -Force $dist
    }
    New-Item -ItemType Directory -Force -Path $dist | Out-Null
    New-Item -ItemType Directory -Force -Path $pluginDst | Out-Null
    Copy-Item (Join-Path $Repo "build\Mirror.exe") (Join-Path $dist "Mirror.exe")
    Copy-GccRuntime $dist
    $font = Join-Path $Repo "assets\fa-solid-900.woff2"
    if (Test-Path $font) {
        Copy-Item -Force $font (Join-Path $dist "fa-solid-900.woff2")
    }
    Get-ChildItem (Join-Path $Ucrt "bin\libgst*.dll") | ForEach-Object {
        Copy-Item -Force $_.FullName (Join-Path $dist $_.Name)
    }
    Get-ChildItem (Join-Path $Ucrt "bin\libgio*.dll") | ForEach-Object {
        Copy-Item -Force $_.FullName (Join-Path $dist $_.Name)
    }
    Get-ChildItem (Join-Path $Ucrt "bin\libssl*.dll") | ForEach-Object {
        Copy-Item -Force $_.FullName (Join-Path $dist $_.Name)
    }
    $seen = @{}
    Add-DllDeps (Join-Path $dist "Mirror.exe") $dist $seen
    Get-ChildItem (Join-Path $dist "libgst*.dll") | ForEach-Object {
        Add-DllDeps $_.FullName $dist $seen
    }
    Get-ChildItem $pluginSrc -File | ForEach-Object {
        Copy-Item -Force $_.FullName (Join-Path $pluginDst $_.Name)
        if ($_.Extension -eq ".dll") {
            Add-DllDeps $_.FullName $dist $seen
        }
    }
}

switch ($Action) {
    "bootstrap" { Invoke-Bootstrap }
    "build" { Invoke-Build }
    "package" { Invoke-Package }
}
