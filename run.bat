@echo off
rem Usage: run.bat [dbg|opt|ship] [game args...]   (default: dbg)
rem Game args: -windowed / -fullscreen, -vsync / -novsync override the per-config defaults.
rem dbg and opt also take the dev driver's args: see "Dev driver" in CLAUDE.md.
rem The game runs with __generated\data as its working directory, so asset paths are
rem relative to it (a shipped build has the exe and the data in the same folder).
setlocal

set CFG=%~1
if "%CFG%"=="" set CFG=dbg

if /I not "%CFG%"=="dbg" if /I not "%CFG%"=="opt" if /I not "%CFG%"=="ship" (
    echo Unknown config "%CFG%". Usage: run.bat [dbg^|opt^|ship] [args...]
    exit /b 1
)

set EXE=%~dp0__generated\bin\%CFG%\game.exe
set DATA_DIR=%~dp0__generated\data
if not exist "%EXE%" (
    echo "%EXE%" not found. Run: build.bat %CFG%
    exit /b 1
)
if not exist "%DATA_DIR%" (
    echo "%DATA_DIR%" not found. Run: cook.bat
    exit /b 1
)

rem Collect everything after the config as game args.
set ARGS=
:collect
shift
if "%~1"=="" goto launch
set ARGS=%ARGS% %1
goto collect

:launch
pushd "%DATA_DIR%"
"%EXE%"%ARGS%
set RESULT=%ERRORLEVEL%
popd
exit /b %RESULT%
