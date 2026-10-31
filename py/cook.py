"""Cooks the game's assets into __generated/data. Invoked by cook.bat (which checks the tools).

Usage: python cook.py [--force] [--debug-shaders]

  --force          re-cook everything, even outputs that are up to date
  --debug-shaders  compile shaders without optimization and with embedded debug info (for PIX /
                   RenderDoc); the mode is remembered, switching it recompiles every shader

An output is up to date when it's newer than its source and than everything that produces it
(packer script, anim_cook.exe, shader includes); only stale outputs are cooked.

Shaders: shaders/<name>.<stage>.hlsl -> data/shaders/<name>.<stage>.cso
  stage vs = vertex shader (vs_5_0), ps = pixel shader (ps_5_0); entry point is main().
  *.hlsli and *.h files in shaders/ are includes only (a change recompiles every shader).
Textures: assets/<path>.png -> data/<path>.tex (py/pack_texture.py)
Fonts: assets/fonts/<name>.ttf -> data/fonts/<name>.font + .tex (py/pack_font.py)
Meshes: assets/<path>.blend -> data/<path>.mesh (py/pack_mesh.py inside Blender; BLENDER env
  var = path to blender.exe, else blender on PATH, else DEFAULT_BLENDER)
Skeletal: assets/animated/<path>.blend -> data/animated/<path>.skel / .<action>.anim / .skin
  (py/pack_skeletal.py inside Blender -> __generated/intermediate/<path>.skraw -> anim_cook.exe,
  built by build_tools.bat when missing)
"""

import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PY_DIR = os.path.join(ROOT, "py")
SHADER_SRC = os.path.join(ROOT, "shaders")
ASSET_SRC = os.path.join(ROOT, "assets")
GENERATED = os.path.join(ROOT, "__generated")
DATA = os.path.join(GENERATED, "data")
SHADER_OUT = os.path.join(DATA, "shaders")
INTERMEDIATE = os.path.join(GENERATED, "intermediate")
ANIM_COOK = os.path.join(GENERATED, "tools", "anim_cook.exe")
SHADER_MODE_FILE = os.path.join(SHADER_OUT, "shader_mode.txt")

DEFAULT_BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"

TEXTURE_PACKER = os.path.join(PY_DIR, "pack_texture.py")
FONT_PACKER = os.path.join(PY_DIR, "pack_font.py")
MESH_PACKER = os.path.join(PY_DIR, "pack_mesh.py")
SKELETAL_PACKER = os.path.join(PY_DIR, "pack_skeletal.py")

PROFILES = {"vs": "vs_5_0", "ps": "ps_5_0"}


def mtime(path: str) -> float:
    try:
        return os.path.getmtime(path)
    except OSError:
        return 0.0


def is_stale(output: str, inputs: list[str], force: bool) -> bool:
    if force or not os.path.exists(output):
        return True
    newest_input = max(mtime(p) for p in inputs)
    return mtime(output) < newest_input


def run(args: list[str], quiet: bool = False) -> bool:
    # quiet: drop stdout, which is noise for fxc (banner) and Blender (log); errors and the
    # packers' summaries go to stderr and stay visible.
    result = subprocess.run(args, stdout=subprocess.DEVNULL if quiet else None)
    return result.returncode == 0


def cook_shaders(force: bool, debug: bool) -> tuple[int, int, bool]:
    os.makedirs(SHADER_OUT, exist_ok=True)

    mode = "debug" if debug else "release"
    try:
        with open(SHADER_MODE_FILE) as f:
            force = force or f.read().strip() != mode
    except OSError:
        force = True

    names = sorted(os.listdir(SHADER_SRC))
    includes = [os.path.join(SHADER_SRC, n) for n in names if n.endswith((".hlsli", ".h"))]
    flags = ["/Od", "/Zi"] if debug else ["/O3"]  # fxc embeds /Zi debug info in the bytecode

    cooked = skipped = 0
    ok = True
    for name in names:
        if not name.endswith(".hlsl"):
            continue
        base = name[: -len(".hlsl")]  # "<name>.<stage>"
        stage = os.path.splitext(base)[1][1:].lower()
        profile = PROFILES.get(stage)
        if not profile:
            print(f"  {name}: unknown shader stage '{stage}' (expected .vs.hlsl or .ps.hlsl)", file=sys.stderr)
            ok = False
            continue

        src = os.path.join(SHADER_SRC, name)
        out = os.path.join(SHADER_OUT, base + ".cso")
        if not is_stale(out, [src] + includes, force):
            skipped += 1
            continue
        print(f"  {name}")
        if run(["fxc", "/nologo", "/T", profile, "/E", "main", "/WX", *flags, "/Fo", out, src], quiet=True):
            cooked += 1
        else:
            ok = False

    if ok:
        with open(SHADER_MODE_FILE, "w") as f:
            f.write(mode)
    return cooked, skipped, ok


def find_assets(extension: str) -> list[str]:
    """Paths relative to assets/, without extension, of every asset with `extension`."""
    found = []
    for dirpath, _, files in os.walk(ASSET_SRC):
        for name in files:
            if name.lower().endswith(extension):
                rel = os.path.relpath(os.path.join(dirpath, name), ASSET_SRC)
                found.append(os.path.splitext(rel)[0])
    return sorted(found)


def cook_textures(force: bool) -> tuple[int, int, bool]:
    cooked = skipped = 0
    ok = True
    for rel in find_assets(".png"):
        src = os.path.join(ASSET_SRC, rel + ".png")
        out = os.path.join(DATA, rel + ".tex")
        if not is_stale(out, [src, TEXTURE_PACKER], force):
            skipped += 1
            continue
        if run([sys.executable, TEXTURE_PACKER, src, out]):
            cooked += 1
        else:
            ok = False
    return cooked, skipped, ok


def cook_fonts(force: bool) -> tuple[int, int, bool]:
    cooked = skipped = 0
    ok = True
    for rel in find_assets(".ttf"):
        src = os.path.join(ASSET_SRC, rel + ".ttf")
        stem = os.path.join(DATA, rel)
        if not is_stale(stem + ".font", [src, FONT_PACKER, TEXTURE_PACKER], force):
            skipped += 1
            continue
        if run([sys.executable, FONT_PACKER, src, stem, os.path.join(INTERMEDIATE, rel + ".png")]):
            cooked += 1
        else:
            ok = False
    return cooked, skipped, ok


def find_blender() -> str:
    blender = os.environ.get("BLENDER")
    if blender:
        return blender
    if shutil.which("blender"):
        return "blender"
    return DEFAULT_BLENDER


def ensure_anim_cook() -> bool:
    if os.path.exists(ANIM_COOK):
        return True
    print("  building anim_cook.exe (build_tools.bat)")
    result = subprocess.run(["cmd", "/c", os.path.join(ROOT, "build_tools.bat")], stdout=subprocess.DEVNULL)
    return result.returncode == 0 and os.path.exists(ANIM_COOK)


def cook_blends(force: bool) -> tuple[int, int, int, bool]:
    blender = find_blender()
    meshes = skeletal = skipped = 0
    ok = True
    for rel in find_assets(".blend"):
        src = os.path.join(ASSET_SRC, rel + ".blend")
        animated = rel.lower().startswith("animated" + os.sep)

        if animated:
            # The .skel stands for all of the asset's outputs (anim_cook writes them together).
            out = os.path.join(DATA, rel + ".skel")
            if not is_stale(out, [src, SKELETAL_PACKER, ANIM_COOK], force) and os.path.exists(ANIM_COOK):
                skipped += 1
                continue
            if not ensure_anim_cook():
                print(f"  {rel}.blend: anim_cook.exe couldn't be built", file=sys.stderr)
                ok = False
                continue
            os.makedirs(os.path.dirname(out), exist_ok=True)
            skraw = os.path.join(INTERMEDIATE, rel + ".skraw")
            packer = SKELETAL_PACKER
            export_target = skraw
        else:
            out = os.path.join(DATA, rel + ".mesh")
            if not is_stale(out, [src, MESH_PACKER], force):
                skipped += 1
                continue
            packer = MESH_PACKER
            export_target = out

        try:
            exported = run([blender, "--background", "--factory-startup", src, "--python-exit-code", "1",
                            "--python", packer, "--", export_target], quiet=True)
        except FileNotFoundError:
            exported = False
        if not exported:
            print(f"  {rel}.blend: export failed (is Blender installed? set BLENDER to blender.exe's path)",
                  file=sys.stderr)
            ok = False
            continue

        if animated:
            if run([ANIM_COOK, skraw, os.path.join(DATA, rel)]):
                skeletal += 1
            else:
                ok = False
        else:
            meshes += 1
    return meshes, skeletal, skipped, ok


def main() -> int:
    args = sys.argv[1:]
    unknown = [a for a in args if a not in ("--force", "--debug-shaders")]
    if unknown:
        print(f"cook.py: unknown arguments {unknown}", file=sys.stderr)
        print(__doc__.splitlines()[2], file=sys.stderr)
        return 2
    force = "--force" in args
    debug_shaders = "--debug-shaders" in args

    print(f"Cooking -> {DATA}" + (" (debug shaders)" if debug_shaders else ""))
    shaders, shaders_skipped, shaders_ok = cook_shaders(force, debug_shaders)
    textures, textures_skipped, textures_ok = cook_textures(force)
    fonts, fonts_skipped, fonts_ok = cook_fonts(force)
    meshes, skeletal, blends_skipped, blends_ok = cook_blends(force)

    if not (shaders_ok and textures_ok and fonts_ok and blends_ok):
        print("Cook FAILED")
        return 1
    up_to_date = shaders_skipped + textures_skipped + fonts_skipped + blends_skipped
    print(f"Cook OK [{shaders} shaders, {textures} textures, {fonts} fonts, {meshes} meshes, {skeletal} skeletal; "
          f"{up_to_date} up to date]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
