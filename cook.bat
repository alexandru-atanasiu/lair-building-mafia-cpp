@echo off
rem Usage: cook.bat [--force] [--debug-shaders]
rem Cooks all game assets into __generated\data (the game's working directory at runtime).
rem Only stale outputs are cooked: --force re-cooks everything. --debug-shaders compiles the
rem shaders without optimization and with debug info (for PIX / RenderDoc captures).
rem Sets up the Visual Studio x64 environment itself when needed (env.bat).
rem
rem The work is done by py\cook.py (its docstring lists every asset type):
rem   shaders\<name>.<stage>.hlsl -> __generated\data\shaders\<name>.<stage>.cso (fxc)
rem   assets\<path>.png           -> __generated\data\<path>.tex (py\pack_texture.py)
rem   assets\fonts\<name>.ttf      -> __generated\data\fonts\<name>.font + .tex (py\pack_font.py)
rem   assets\<path>.blend         -> __generated\data\<path>.mesh (py\pack_mesh.py in Blender)
rem   assets\animated\<path>.blend -> __generated\data\animated\<path>.skel / .<action>.anim / .skin
rem     (py\pack_skeletal.py in Blender, then anim_cook.exe, built by build_tools.bat when missing)
rem Blender: the BLENDER environment variable (full path to blender.exe), else on PATH, else the
rem default install folder (see py\cook.py).
setlocal

call "%~dp0env.bat"
if errorlevel 1 exit /b 1

where fxc >nul 2>nul || (
    echo fxc.exe not found: env.bat should have put it on PATH.
    exit /b 1
)
where python >nul 2>nul || (
    echo python.exe not found. Install Python 3 ^(with Pillow^) and add it to PATH.
    exit /b 1
)

rem -u: unbuffered, so our lines and the tools' output appear in order.
python -u "%~dp0py\cook.py" %*
exit /b %ERRORLEVEL%
