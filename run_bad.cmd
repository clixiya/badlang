@echo off
setlocal

set "FILE=%~1"
if "%FILE%"=="" set "FILE=examples/01-basics/quick_start_demo.bad"

if exist ".\bad.exe" (
    set "BIN=.\bad.exe"
) else (
    set "BIN=bad"
)

if "%~1"=="" (
    "%BIN%" "%FILE%" --config examples/.badrc
) else (
    shift
    "%BIN%" "%FILE%" --config examples/.badrc %*
)

exit /b %ERRORLEVEL%
