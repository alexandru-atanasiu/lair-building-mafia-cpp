@echo off
rem Puts the Visual Studio x64 tools (cl, link, fxc) on PATH when the shell doesn't have them yet.
rem Called by build.bat, build_tools.bat and cook.bat. Set VCVARS64 to use another install.
if defined VSCMD_VER if defined INCLUDE exit /b 0

if not defined VCVARS64 set "VCVARS64=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS64%" (
    echo vcvars64.bat not found at "%VCVARS64%". Set VCVARS64 to its path, or run from a Visual Studio x64 Developer Command Prompt.
    exit /b 1
)
call "%VCVARS64%" >nul
if not defined INCLUDE (
    echo "%VCVARS64%" did not set up the environment.
    exit /b 1
)
exit /b 0
