#!/usr/bin/env python3
"""Bundle SDL2 into a macOS app and create a distributable studio ZIP."""
import json
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def command(*args):
    return subprocess.check_output(args, text=True)


def copy_clean(source, target):
    # Do not carry extended attributes/AppleDouble files from an external exFAT drive.
    target.mkdir(parents=True, exist_ok=True)
    for path in source.rglob("*"):
        relative = path.relative_to(source)
        if any(part.startswith("._") or part == ".DS_Store" for part in relative.parts):
            continue
        out = target / relative
        if path.is_dir():
            out.mkdir(parents=True, exist_ok=True)
        elif path.is_file():
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_bytes(path.read_bytes())
            out.chmod(path.stat().st_mode & 0o777)


def main():
    source = ROOT / "simulator/build/bambu_studio.app"
    dependencies = command("otool", "-L", str(source / "Contents/MacOS/bambu_studio"))
    if "asan" in dependencies or "ubsan" in dependencies:
        raise SystemExit("Build with BAMBU_SANITIZERS=OFF first.")
    linked = [line.strip().split(" (", 1)[0] for line in dependencies.splitlines()[1:]]
    sdl = next((Path(line) for line in linked if "libSDL2" in line), None)
    if not sdl or not sdl.is_file():
        raise SystemExit("Linked SDL2 library not found.")
    dist = ROOT / "dist"
    dist.mkdir(exist_ok=True)
    destination = dist / "Bambu Bot Studio.app"
    archive = dist / f"bambu-bot-studio-macos-{platform.machine()}-v0.2.0.zip"
    with tempfile.TemporaryDirectory(prefix="bambu-studio-package-") as temporary:
        staged = Path(temporary) / destination.name
        copy_clean(source, staged)
        frameworks = staged / "Contents/Frameworks"
        frameworks.mkdir(exist_ok=True)
        bundled = frameworks / sdl.name
        bundled.write_bytes(sdl.read_bytes())
        bundled.chmod(0o755)
        binary = staged / "Contents/MacOS/bambu_studio"
        subprocess.run(["install_name_tool", "-id", "@rpath/" + sdl.name, str(bundled)], check=True)
        subprocess.run(["install_name_tool", "-change", str(sdl), "@executable_path/../Frameworks/" + sdl.name, str(binary)], check=True)
        for artifact in (bundled, binary):
            final = command("otool", "-L", str(artifact))
            if "/opt/homebrew" in final or "/usr/local/" in final:
                raise SystemExit("Unexpected unbundled dependency remains.")
        resources = staged / "Contents/Resources"
        resources.mkdir(exist_ok=True)
        for notice in (ROOT / "simulator/licenses").iterdir():
            if notice.is_file() and not notice.name.startswith("._"):
                (resources / notice.name).write_bytes(notice.read_bytes())
        (resources / "THIRD_PARTY.txt").write_text(
            "LVGL 9.5.0 (MIT): https://github.com/lvgl/lvgl/tree/v9.5.0\n"
            "SDL2 (zlib): https://github.com/libsdl-org/SDL/tree/SDL2\n"
            "Source: https://github.com/Hdchipeo/bambu-bot\n")
        subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(staged)], check=True)
        subprocess.run(["codesign", "--verify", "--deep", "--strict", str(staged)], check=True)
        subprocess.run([str(binary), "--self-test"], check=True)
        subprocess.run(["ditto", "-c", "-k", "--norsrc", "--noextattr", "--keepParent", str(staged), str(archive)], check=True)
        with zipfile.ZipFile(archive) as package:
            if package.testzip() is not None or any("/._" in name for name in package.namelist()):
                raise SystemExit("Package integrity or AppleDouble metadata check failed.")
        if destination.exists():
            shutil.rmtree(destination)
        copy_clean(staged, destination)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(destination)], check=True)
    print(json.dumps({"app": str(destination), "archive": str(archive), "architecture": platform.machine()}))


if __name__ == "__main__":
    main()
