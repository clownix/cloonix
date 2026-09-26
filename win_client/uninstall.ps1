# uninstall.ps1 - remove what install.ps1 put on the machine.
# Three ways in, all of them this file: Add or remove programs, uninstall.cmd
# in the install directory, or this run by hand. Everything it touches
# belongs to the user -- a directory under LOCALAPPDATA, two environment
# variables and the PATH of the user, one start menu shortcut, one key of
# HKCU -- so no administrator is asked for and nothing of the machine is
# read or written.
# WHERE to remove from is asked of the registry first and of the directory
# this file sits in only last, because this file sits in two places: the
# installation, and the package it was installed from. A copy run from the
# package, or from a desktop, must not take that directory with it.
# And whatever the answer, a directory that does not hold cloonix and
# cloonix.cmd is not removed. Those two names are written by the installer
# and by nothing else, so they are what tells an installation from a
# directory that merely has this script in it.
[CmdletBinding()]
param(
    # Where the client was installed. Found on its own when not given.
    [string] $Prefix,
    # Remove without asking. Add or remove programs has asked already, but
    # it is one keystroke and this deletes a directory.
    [switch] $Force,
    # uninstall.cmd passes this so a double clicked window stays up.
    [switch] $Pause
)

$ErrorActionPreference = "Continue"
$key = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\cloonix-client"
$lnk = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Cloonix client.lnk"

function Say ($m) { Write-Host $m }

function Test-Install ($dir) {
    if (-not $dir) { return $false }
    if (-not (Test-Path -PathType Container $dir)) { return $false }
    return (Test-Path (Join-Path $dir "cloonix")) -and
           (Test-Path (Join-Path $dir "cloonix.cmd"))
}

# 1. Which directory.
$asked = $Prefix
$where = $null
$tries = New-Object System.Collections.Generic.List[string]
if ($Prefix) {
    # Named on the command line: that one and no other. Answering with a
    # different directory than the one asked for would be worse than
    # refusing, however right the other one is.
    $tries.Add($Prefix)
} else {
    if (Test-Path $key) {
        try { $tries.Add((Get-ItemProperty $key).InstallLocation) } catch { }
    }
    $var = [Environment]::GetEnvironmentVariable("CLOONIX_CLIENT", "User")
    if ($var) { $tries.Add((Split-Path -Parent $var)) }
    $tries.Add($PSScriptRoot)
}

foreach ($t in $tries) {
    if (Test-Install $t) { $where = (Resolve-Path $t).Path; break }
}

if ($asked -and -not $where) {
    Say ""
    Say "$asked is not an installed cloonix client: it has no cloonix and no"
    Say "cloonix.cmd in it. Nothing was removed."
    if ($Pause) { Read-Host "press enter to close" | Out-Null }
    exit 1
}

# 2. What is about to go.
Say ""
Say "Removing the cloonix client:"
Say ""
if ($where) {
    Say "   $where"
    Say "   and everything in it, a demo of your own saved there included"
} else {
    Say "   no install directory found -- only the leftovers below"
}
Say "   the PATH entry, CLOONIX_CLIENT and CLOONIX_DEMO_DIR"
Say "   the start menu entry, and the Add or remove programs entry"
Say ""
Say "Nothing else is touched: no python, no other directory, nothing of the"
Say "machine."
Say ""
if (-not $Force) {
    $answer = Read-Host "remove it? (y/N)"
    if ($answer -notmatch '^(y|Y|yes|YES)$') {
        Say ""
        Say "nothing was removed."
        if ($Pause) { Read-Host "press enter to close" | Out-Null }
        exit 0
    }
    Say ""
}

# 3. The environment first: if the directory will not go, the PATH must
#    still not be left pointing at it.
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($userPath -and $where) {
    $parts = @($userPath.Split(";") | Where-Object { $_ -ne "" -and $_ -ne $where })
    if ($parts.Count -ne @($userPath.Split(";") | Where-Object { $_ -ne "" }).Count) {
        [Environment]::SetEnvironmentVariable("Path", ($parts -join ";"), "User")
        Say "   PATH entry removed"
    }
}
foreach ($v in @("CLOONIX_CLIENT", "CLOONIX_DEMO_DIR")) {
    if ([Environment]::GetEnvironmentVariable($v, "User")) {
        [Environment]::SetEnvironmentVariable($v, $null, "User")
        Say "   $v unset"
    }
}
if (Test-Path $lnk) { Remove-Item -Force $lnk; Say "   start menu entry removed" }
if (Test-Path $key) { Remove-Item -Recurse -Force $key; Say "   uninstall entry removed" }

# 4. The directory. Out of it first: a current directory inside a tree is
#    what stops the tree from going.
if ($where) {
    Set-Location $env:TEMP
    try {
        Remove-Item -Recurse -Force $where
        Say "   $where removed"
    } catch {
        Say ""
        Say "could not remove $where"
        Say "   $($_.Exception.Message)"
        Say ""
        Say "Something is still using it -- a cloonix session of some kind, or"
        Say "a terminal sitting in that directory. Close it and delete the"
        Say "directory by hand; everything else is already undone."
        if ($Pause) { Read-Host "press enter to close" | Out-Null }
        exit 1
    }
}

Say ""
Say "Done. A terminal opened from now on has no cloonix in it; one that was"
Say "already open still has the old PATH until it is closed."
if ($Pause) { Read-Host "press enter to close" | Out-Null }
exit 0
