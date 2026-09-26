# install.ps1 - put the cloonix client on a windows 10 or windows 11 machine.
#   install.bat                        double click, or from a cmd window
#   powershell -File install.ps1       the same thing without the wrapper
# Installs python source only (client, cloonix_*.py, demos) under LOCALAPPDATA,
# no admin. Python >= 3.7 is found via the py launcher, PATH and registry, each
# candidate run for its version (the store stub answers nothing).
# Uninstall is uninstall.ps1, installed beside the client and registered in
# Add or remove programs.
[CmdletBinding()]
param(
    # Where it goes. The default needs no administrator and is per user.
    [string] $Prefix = "$env:LOCALAPPDATA\Programs\cloonix",
    # install.bat passes this so a double clicked window stays up.
    [switch] $Pause
)

$ErrorActionPreference = "Stop"
$MinPython = [version] "3.7"

function Say    ($m) { Write-Host $m }
function Step   ($m) { Write-Host "" ; Write-Host "== $m" -ForegroundColor Cyan }
function Warn   ($m) { Write-Host "   $m" -ForegroundColor Yellow }

function Fail ($m) {
    Write-Host ""
    Write-Host "install failed: $m" -ForegroundColor Red
    Write-Host ""
    if ($Pause) { Read-Host "press enter to close" | Out-Null }
    exit 1
}

# 1. The machine.
Step "windows"
$os = [Environment]::OSVersion.Version
$cur = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion"
$build = 0
try { $build = [int](Get-ItemProperty $cur).CurrentBuildNumber } catch { }
if ($os.Major -lt 10) {
    Fail "this installer is for windows 10 and windows 11, and this is $os."
}
$name = if ($build -ge 22000) { "windows 11" } else { "windows 10" }
Say "   $name, build $build"

# 2. What is to be installed, found beside this script or above it.
# Two layouts: the make_win_zip.sh zip (bin/ and demos/ beside), or the source tree (cloonix/ and demos/ above).
Step "what is being installed"
$here = $PSScriptRoot
$srcBin = $null
foreach ($cand in @("$here\bin", "$here\..\cloonix")) {
    if (Test-Path "$cand\cloonix") { $srcBin = (Resolve-Path $cand).Path; break }
}
if (-not $srcBin) {
    Fail "the client is not beside this script. Expected $here\bin\cloonix or $here\..\cloonix\cloonix."
}
$srcDemos = $null
foreach ($cand in @("$here\demos", "$here\..\demos")) {
    if (Test-Path $cand) { $srcDemos = (Resolve-Path $cand).Path; break }
}

$version = "unknown"
$cfg = "$srcBin\cloonix_config.py"
if (Test-Path $cfg) {
    $m = Select-String -Path $cfg -Pattern 'CLOONIX_VERSION=(\S+)' | Select-Object -First 1
    if ($m) { $version = $m.Matches[0].Groups[1].Value }
}
Say "   client   $srcBin"
if ($srcDemos) { Say "   demos    $srcDemos" } else { Warn "no demos beside this script, none will be installed" }
Say "   version  $version"

# 3. The interpreter.
Step "python"

function Ask-Python ($exe) {
    # The version, by running it. A python that cannot run is not a python,
    # and that is exactly what the store stub in WindowsApps is.
    try {
        $out = & $exe -c "import sys;print('%d.%d.%d' % sys.version_info[:3])" 2>$null
    } catch { return $null }
    if ($LASTEXITCODE -ne 0 -or -not $out) { return $null }
    $out = ($out | Select-Object -First 1).Trim()
    if ($out -notmatch '^\d+\.\d+\.\d+$') { return $null }
    try { $full = (& $exe -c "import sys;print(sys.executable)" 2>$null | Select-Object -First 1).Trim() }
    catch { $full = $exe }
    if (-not $full) { $full = $exe }
    return [pscustomobject]@{ Path = $full; Version = [version] $out }
}

$candidates = New-Object System.Collections.Generic.List[string]
# the py launcher, which lists every python it knows of
try {
    foreach ($line in (& py -0p 2>$null)) {
        if ($line -match '([A-Za-z]:\\[^\s].*\.exe)') { $candidates.Add($Matches[1]) }
    }
} catch { }
# the PATH
foreach ($n in @("python.exe", "python3.exe")) {
    try { foreach ($c in (Get-Command $n -All -ErrorAction SilentlyContinue)) { $candidates.Add($c.Source) } }
    catch { }
}
# the registry, where every installer of python.org writes itself
foreach ($root in @("HKCU:\Software\Python", "HKLM:\Software\Python",
                    "HKLM:\Software\WOW6432Node\Python")) {
    if (-not (Test-Path $root)) { continue }
    foreach ($key in (Get-ChildItem -Recurse $root -ErrorAction SilentlyContinue |
                      Where-Object { $_.PSChildName -eq "InstallPath" })) {
        $p = (Get-ItemProperty $key.PSPath)."(default)"
        if ($p) { $candidates.Add((Join-Path $p "python.exe")) }
    }
}

$found = @{}
foreach ($c in $candidates) {
    if (-not $c) { continue }
    $got = Ask-Python $c
    if ($got) { $found[$got.Path.ToLower()] = $got }
}
if ($found.Count -eq 0) {
    Fail @"
no python found.

  The client is python source and needs an interpreter, $MinPython or later.
  Install one from https://www.python.org/downloads/windows/ and tick
  "Add python.exe to PATH", then run this installer again.

  The python of the microsoft store also works, but the python.exe alias
  that opens the store is not a python and is skipped here.
"@
}
foreach ($p in ($found.Values | Sort-Object Version)) { Say "   $($p.Version)  $($p.Path)" }
$python = ($found.Values | Sort-Object Version -Descending | Select-Object -First 1)
if ($python.Version -lt $MinPython) {
    Fail "the newest python here is $($python.Version), and the client needs $MinPython or later."
}
Say ""
Say "   using $($python.Path)"

# 4. The files.
Step "copying to $Prefix"
New-Item -ItemType Directory -Force -Path $Prefix | Out-Null
Copy-Item -Force "$srcBin\cloonix" $Prefix
Copy-Item -Force "$srcBin\cloonix_*.py" $Prefix
$n = (Get-ChildItem "$Prefix\cloonix_*.py").Count
Say "   cloonix and $n cloonix_*.py"
if ($srcDemos) {
    # The shipped demos are replaced, anything else under demos\ is left:
    # a demo of your own saved there survives an upgrade.
    New-Item -ItemType Directory -Force -Path "$Prefix\demos" | Out-Null
    Copy-Item -Recurse -Force "$srcDemos\*" "$Prefix\demos"
    $n = (Get-ChildItem -Recurse "$Prefix\demos" -Filter *.py).Count
    Say "   $n demos"
}
Copy-Item -Force "$here\uninstall.ps1" $Prefix
# A zip that came through a browser marks every file it carried, and the
# mark is what makes windows ask about a file it has just been told to run.
try { Get-ChildItem -Recurse $Prefix | Unblock-File } catch { }

# 5. The shim, which is what "cloonix" on the PATH is.
# It carries the two variables itself as well as the installer setting them,
# so the client works from a window that was open before the install and has
# the old environment in it.
Step "the cloonix command"
$cmd = @(
    '@echo off'
    'setlocal'
    'if not defined CLOONIX_CLIENT set "CLOONIX_CLIENT=%~dp0cloonix"'
    'if not defined CLOONIX_DEMO_DIR set "CLOONIX_DEMO_DIR=%~dp0demos"'
    ('"' + $python.Path + '" "%~dp0cloonix" %*')
) -join "`r`n"
# ascii and no byte order mark: a mark at the top of a .cmd is printed by
# cmd.exe as three characters and the first line does not run.
[System.IO.File]::WriteAllText("$Prefix\cloonix.cmd", $cmd + "`r`n",
                               [System.Text.Encoding]::ASCII)
# One line ending in exit: cmd.exe reads a batch file as it runs it, and the
# next read would be of a file this very call has just deleted.
$un = @(
    '@echo off'
    'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall.ps1" -Pause & exit /b'
) -join "`r`n"
[System.IO.File]::WriteAllText("$Prefix\uninstall.cmd", $un + "`r`n",
                               [System.Text.Encoding]::ASCII)
Say "   $Prefix\cloonix.cmd"

# 6. The environment of the user, not of the machine.
Step "environment"
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if (-not $userPath) { $userPath = "" }
$parts = @($userPath.Split(";") | Where-Object { $_ -ne "" })
if ($parts -notcontains $Prefix) {
    [Environment]::SetEnvironmentVariable(
        "Path", (($parts + $Prefix) -join ";"), "User")
    Say "   PATH               += $Prefix"
} else {
    Say "   PATH               already had it"
}
[Environment]::SetEnvironmentVariable("CLOONIX_CLIENT", "$Prefix\cloonix", "User")
Say "   CLOONIX_CLIENT     = $Prefix\cloonix"
if ($srcDemos) {
    [Environment]::SetEnvironmentVariable("CLOONIX_DEMO_DIR", "$Prefix\demos", "User")
    Say "   CLOONIX_DEMO_DIR   = $Prefix\demos"
}

# 7. The start menu, and Add or remove programs.
Step "start menu and uninstall entry"
try {
    $menu = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs"
    $ws = New-Object -ComObject WScript.Shell
    $lnk = $ws.CreateShortcut("$menu\Cloonix client.lnk")
    $lnk.TargetPath = $env:ComSpec
    $lnk.Arguments = '/k "' + $Prefix + '\cloonix.cmd" --help'
    $lnk.WorkingDirectory = $Prefix
    $lnk.Description = "The cloonix command line client"
    $lnk.Save()
    Say "   Cloonix client"
} catch {
    Warn "no start menu entry: $($_.Exception.Message)"
}
try {
    $key = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\cloonix-client"
    New-Item -Force -Path $key | Out-Null
    $size = [int]((Get-ChildItem -Recurse $Prefix | Measure-Object Length -Sum).Sum / 1024)
    Set-ItemProperty $key -Name DisplayName     -Value "Cloonix client"
    Set-ItemProperty $key -Name DisplayVersion  -Value $version
    Set-ItemProperty $key -Name Publisher       -Value "clownix.net"
    Set-ItemProperty $key -Name InstallLocation -Value $Prefix
    Set-ItemProperty $key -Name UninstallString -Value ("powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$Prefix\uninstall.ps1`" -Pause")
    Set-ItemProperty $key -Name EstimatedSize   -Value $size -Type DWord
    Set-ItemProperty $key -Name NoModify        -Value 1 -Type DWord
    Set-ItemProperty $key -Name NoRepair        -Value 1 -Type DWord
    Say "   Add or remove programs: Cloonix client $version"
} catch {
    Warn "no uninstall entry: $($_.Exception.Message)"
}

# 8. Say it works, by running it.
Step "checking the installed client"
# stderr merged into stdout is a terminating error while the preference is
# Stop, so it is put back to Continue for the length of the call.
$keep = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$said = & $python.Path "$Prefix\cloonix" --help 2>&1
$rc = $LASTEXITCODE
$ErrorActionPreference = $keep
$head = ($said | Select-Object -First 1)
if ($rc -ne 0) {
    Fail "the client is in place but would not run: $($said -join ' ')"
}
Say "   $head"

Write-Host ""
Write-Host "cloonix client $version is installed." -ForegroundColor Green
Write-Host @"

  Open a NEW terminal -- this one has the environment from before the
  install -- and try:

      cloonix list                    this client, and any net that answers
      cloonix cli nemo demo              the demos, by name
      cloonix cli nemo demo cloonix/a2b/a2b.py

  The nets, their ports and their passwords come from cloonix_config.py in
  $Prefix.
  It is the config of the cloonix that built this package; if your server
  has another one, copy its cloonix_config.py over that file.

  There is no server here. cloonix start starts one and that is linux only:
  everything above drives a cloonix running on a linux machine, over the
  single tcp port of the net.

  To remove it all: Add or remove programs, or $Prefix\uninstall.cmd
"@
if ($Pause) { Read-Host "press enter to close" | Out-Null }
exit 0
