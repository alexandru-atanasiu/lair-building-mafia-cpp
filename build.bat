@echo off
rem Usage: build.bat [dbg|opt|ship]   (default: dbg)
rem Sets up the Visual Studio x64 environment itself when needed (env.bat).
setlocal

call "%~dp0env.bat"
if errorlevel 1 exit /b 1

set CFG=%~1
if "%CFG%"=="" set CFG=dbg

rem Dev-only features shared by dbg and opt.
set DEV_FEATURES=/DGAME_LOGGING_ENABLED /DGAME_ASSERTS_ENABLED /DGAME_ASSERT_POPUP /DGAME_IMGUI_ENABLED /DGAME_IMGUI_CONSOLE_ENABLED /DGAME_DEV_TOOLS_ENABLED

if /I "%CFG%"=="dbg" (
    set CFG_CFLAGS=/Od /Zi /MTd /DGAME_DBG /D_DEBUG %DEV_FEATURES%
    set CFG_LFLAGS=/DEBUG:FULL
    set USE_IMGUI=1
) else if /I "%CFG%"=="opt" (
    set CFG_CFLAGS=/O2 /Zi /MT /DGAME_OPT /DNDEBUG %DEV_FEATURES%
    set CFG_LFLAGS=/DEBUG:FULL /OPT:REF /OPT:ICF
    set USE_IMGUI=1
) else if /I "%CFG%"=="ship" (
    rem Symbols don't change the generated code: the .pdb is kept next to the exe (but not
    rem shipped) to read crash dumps. PDBALTPATH keeps the build machine's path out of the exe.
    set CFG_CFLAGS=/O2 /Zi /MT /DGAME_SHIP /DNDEBUG
    set CFG_LFLAGS=/DEBUG:FULL /PDBALTPATH:%%_PDB%% /OPT:REF /OPT:ICF
    set USE_IMGUI=0
) else (
    echo Unknown config "%CFG%". Usage: build.bat [dbg^|opt^|ship]
    exit /b 1
)

set ROOT=%~dp0
set OUT=%ROOT%__generated\bin\%CFG%
set OBJ=%OUT%\obj
set IMGUI_DIR=%ROOT%vendor\imgui
set OZZ_DIR=%ROOT%vendor\ozz
if not exist "%OBJ%" mkdir "%OBJ%"
rem Vendor objects get their own folders: they're kept between builds (see below), and some
rem ozz basenames clash with ours (log.cc / log.cpp).
if not exist "%OBJ%\ozz" mkdir "%OBJ%\ozz"
if not exist "%OBJ%\imgui" mkdir "%OBJ%\imgui"

set DEFINES=/DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX
set COMMON_CFLAGS=/nologo /c /std:c++20 /permissive- /EHsc /GR- /utf-8 /MP /FS %DEFINES% %CFG_CFLAGS%
rem Our code: strict warnings. Third-party code: default warnings, not treated as errors.
rem ozz headers are "external" for our code: their warnings don't trip /W4 /WX.
set CFLAGS=%COMMON_CFLAGS% /W4 /WX /I"%ROOT%include" /external:W0 /external:I"%OZZ_DIR%\include"
set VENDOR_CFLAGS=%COMMON_CFLAGS% /W3 /I"%IMGUI_DIR%"
set OZZ_CFLAGS=%COMMON_CFLAGS% /W3 /D_CRT_SECURE_NO_WARNINGS /I"%OZZ_DIR%\include" /I"%OZZ_DIR%\src"
if "%USE_IMGUI%"=="1" set CFLAGS=%CFLAGS% /I"%IMGUI_DIR%"

rem Plain main() entry point while still being a GUI (no console) app.
set LFLAGS=/nologo /INCREMENTAL:NO /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /MANIFEST:EMBED %CFG_LFLAGS%
rem windowscodecs + uuid: the dev driver's PNG screenshots (nothing is pulled in for ship).
set LIBS=user32.lib d3d11.lib dxgi.lib comctl32.lib dbghelp.lib shell32.lib ole32.lib windowscodecs.lib uuid.lib

rem Gather sources (our object files go to one dir, so basenames must be unique across src\).
setlocal EnableDelayedExpansion
set SOURCES=
for /r "%ROOT%src" %%f in (*.cpp) do set SOURCES=!SOURCES! "%%f"
set IMGUI_SOURCES=
for %%f in ("%IMGUI_DIR%\*.cpp" "%IMGUI_DIR%\backends\*.cpp") do set IMGUI_SOURCES=!IMGUI_SOURCES! "%%f"
rem ozz runtime only (base + animation runtime); the offline part is for tools (build_tools.bat).
set OZZ_SOURCES=
for /r "%OZZ_DIR%\src\base" %%f in (*.cc) do set OZZ_SOURCES=!OZZ_SOURCES! "%%f"
for %%f in ("%OZZ_DIR%\src\animation\runtime\*.cc") do set OZZ_SOURCES=!OZZ_SOURCES! "%%f"

echo Building [%CFG%] -^> %OUT%

rem Work from inside the obj dir so .obj/.pdb intermediates land there. Our objects start clean
rem so objects from a previous build with different features never get linked in.
pushd "%OBJ%"
del /q *.obj 2>nul

cl %CFLAGS% !SOURCES!
if errorlevel 1 goto failed

rem Vendor code only changes when it's updated: its objects are reused while their stamp (the
rem compiler flags + the vendor's VERSION.txt) is unchanged. Delete obj\ozz or obj\imgui to force.
call :vendor ozz "%OZZ_DIR%\VERSION.txt" OZZ_CFLAGS OZZ_SOURCES
if errorlevel 1 goto failed

set LINK_OBJS=*.obj ozz\*.obj
if not "%USE_IMGUI%"=="1" goto link
call :vendor imgui "%IMGUI_DIR%\VERSION.txt" VENDOR_CFLAGS IMGUI_SOURCES
if errorlevel 1 goto failed
set LINK_OBJS=%LINK_OBJS% imgui\*.obj

:link
link %LFLAGS% %LINK_OBJS% %LIBS% /OUT:"%OUT%\game.exe"
if errorlevel 1 goto failed

popd
echo Build OK [%CFG%]
exit /b 0

:failed
popd
echo Build FAILED [%CFG%]
exit /b 1

rem :vendor <name> <VERSION.txt> <cflags variable> <sources variable>
rem Compiles into obj\<name>\ unless its objects are up to date. Run from the obj dir.
:vendor
set V_FLAGS=!%~3!
set V_SOURCES=!%~4!
> "%~1\stamp.new" (echo !V_FLAGS!& type "%~2")
fc /b "%~1\stamp.new" "%~1\stamp.txt" >nul 2>nul
if not errorlevel 1 if exist "%~1\*.obj" (
    del "%~1\stamp.new"
    echo %~1: up to date
    exit /b 0
)
del /q "%~1\*.obj" "%~1\stamp.txt" 2>nul
cl !V_FLAGS! /Fo.\%~1\ !V_SOURCES!
if errorlevel 1 exit /b 1
move /y "%~1\stamp.new" "%~1\stamp.txt" >nul
exit /b 0
