@echo off
rem Usage: build_tools.bat
rem Builds the asset tools into __generated\tools (cook.bat builds them when missing; rerun
rem this after changing tools\ or vendor\ozz).
rem Sets up the Visual Studio x64 environment itself when needed (env.bat).
rem
rem   anim_cook.exe  .skraw (Blender export) -> ozz skeleton/animations + skinned mesh
setlocal EnableDelayedExpansion

call "%~dp0env.bat"
if errorlevel 1 exit /b 1

set ROOT=%~dp0
set OUT=%ROOT%__generated\tools
set OBJ=%OUT%\obj
set OZZ_DIR=%ROOT%vendor\ozz
if not exist "%OBJ%\ozz" mkdir "%OBJ%\ozz"

set COMMON_CFLAGS=/nologo /c /std:c++20 /permissive- /EHsc /GR- /utf-8 /MP /FS /O2 /MT /DNDEBUG /DWIN32_LEAN_AND_MEAN /DNOMINMAX
rem Our code: strict warnings (ozz headers are external). ozz itself: default warnings.
set CFLAGS=%COMMON_CFLAGS% /W4 /WX /I"%ROOT%include" /external:W0 /external:I"%OZZ_DIR%\include"
set OZZ_CFLAGS=%COMMON_CFLAGS% /W3 /D_CRT_SECURE_NO_WARNINGS /I"%OZZ_DIR%\include" /I"%OZZ_DIR%\src"

set OZZ_SOURCES=
for /r "%OZZ_DIR%\src" %%f in (*.cc) do set OZZ_SOURCES=!OZZ_SOURCES! "%%f"

echo Building tools -^> %OUT%

pushd "%OBJ%"
del /q *.obj ozz\*.obj 2>nul

cl %CFLAGS% "%ROOT%tools\anim_cook\anim_cook.cpp"
if errorlevel 1 goto failed

cl %OZZ_CFLAGS% /Fo.\ozz\ !OZZ_SOURCES!
if errorlevel 1 goto failed

link /nologo /INCREMENTAL:NO /SUBSYSTEM:CONSOLE anim_cook.obj ozz\*.obj /OUT:"%OUT%\anim_cook.exe"
if errorlevel 1 goto failed

popd
echo Build tools OK
exit /b 0

:failed
popd
echo Build tools FAILED
exit /b 1
