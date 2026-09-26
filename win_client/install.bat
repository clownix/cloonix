@echo off
rem --------------------------------------------------------------------------
rem install.bat - what a double click runs, and what a cmd window can run.
rem
rem Nothing happens here: powershell does the install. The two options are
rem the ones a downloaded script needs -- no profile of the user is loaded,
rem and the execution policy is bypassed for this one call only, which is
rem what -File on the command line means and does not change any setting.
rem --------------------------------------------------------------------------
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Pause %*
