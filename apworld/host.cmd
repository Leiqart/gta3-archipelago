@echo off
rem Generates a seed from the YAML files of players\ and hosts it on a local Archipelago server.
rem Usage: host.cmd [Archipelago folder]; type /exit in this window, or close it, to stop the server.
setlocal

set "AP_DIR=%~1"
if "%AP_DIR%"=="" set "AP_DIR=%ARCHIPELAGO_DIR%"
if "%AP_DIR%"=="" set "AP_DIR=C:\ProgramData\Archipelago"
set "OUT_DIR=%~dp0output"
if not exist "%AP_DIR%\ArchipelagoServer.exe" (echo Archipelago not found in %AP_DIR% & exit /b 1)

rem Start from an empty output folder, so the server always hosts the seed generated just now.
if exist "%OUT_DIR%" rmdir /s /q "%OUT_DIR%"
mkdir "%OUT_DIR%"
"%AP_DIR%\ArchipelagoGenerate.exe" --player_files_path "%~dp0players" --outputpath "%OUT_DIR%" || (echo Generation failed & exit /b 1)

rem The generator writes AP_<seed>.zip; unpack it so the spoiler log can be opened directly.
for %%f in ("%OUT_DIR%\AP_*.zip") do set "SEED_ZIP=%%f"
if not defined SEED_ZIP (echo No seed was generated & exit /b 1)
"%SystemRoot%\System32\tar.exe" -x -f "%SEED_ZIP%" -C "%OUT_DIR%" || (echo Unpacking failed & exit /b 1)
for %%f in ("%OUT_DIR%\*_Spoiler.txt") do echo Spoiler log: %%f

rem The server runs in this window until /exit; --host 127.0.0.1 keeps it reachable from this PC only, at localhost:38281.
"%AP_DIR%\ArchipelagoServer.exe" --host 127.0.0.1 "%SEED_ZIP%"
