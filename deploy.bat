@echo off
rem Usage: deploy.bat [cook args...]   (e.g. deploy.bat --force)
rem Makes a shippable build in __generated\deploy: builds the ship config, cooks the game data
rem and copies the exe and the data into that one folder (the game loads assets relative to
rem the exe's working directory, so the folder is self-contained and can be zipped as is).
rem Extra arguments are passed to cook.bat. game.pdb is not copied (it stays next to the exe
rem in __generated\bin\ship to read crash dumps from a deployed build).
rem Sets up the Visual Studio x64 environment itself when needed (env.bat).
setlocal

set ROOT=%~dp0
set BIN=%ROOT%__generated\bin\ship
set DATA=%ROOT%__generated\data
set DEPLOY=%ROOT%__generated\deploy

call "%ROOT%build.bat" ship
if errorlevel 1 goto failed

call "%ROOT%cook.bat" %*
if errorlevel 1 goto failed

echo Deploying -^> %DEPLOY%
rem /MIR mirrors the data tree (stale files from a previous deploy are removed).
rem shader_mode.txt is cook.bat bookkeeping, not game data.
robocopy "%DATA%" "%DEPLOY%" /MIR /XF shader_mode.txt /NFL /NDL /NJH /NJS /NP
if errorlevel 8 goto failed
copy /y "%BIN%\game.exe" "%DEPLOY%\game.exe" >nul
if errorlevel 1 goto failed

echo Deploy OK: %DEPLOY%\game.exe
exit /b 0

:failed
echo Deploy FAILED
exit /b 1
