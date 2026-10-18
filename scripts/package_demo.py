#!/usr/bin/env python3
"""Package the credential-free default build for manual flashing."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build")
    args = parser.parse_args()
    build = ROOT / args.build_dir
    cache = (build / "CMakeCache.txt").read_text()
    forced_demo = "BAMBU_FORCE_DEMO:BOOL=ON" in cache.splitlines()
    if (ROOT / "main/bambu/local_config.h").exists() and not forced_demo:
        raise SystemExit("Refusing to package a build with local configuration. Remove it and rebuild the default demo first.")
    if forced_demo and b"BAMBU_CREDENTIAL_FREE_DEMO_PROFILE" not in (build / "bambu_bot.bin").read_bytes():
        raise SystemExit("Refusing to package: binary lacks the forced-demo marker. Rebuild first.")
    description = json.loads((build / "project_description.json").read_text())
    version = description["project_version"]
    sources = {
        "bootloader.bin": "bootloader/bootloader.bin",
        "partition-table.bin": "partition_table/partition-table.bin",
        "ota_data_initial.bin": "ota_data_initial.bin",
        "bambu_bot.bin": "bambu_bot.bin",
        "generated_assets.bin": "generated_assets.bin",
    }
    manifest = {"project": "bambu-bot", "version": version, "profile": "deskbot-demo",
                "forced_demo": forced_demo,
                "coffee_preview": "BAMBU_COFFEE_PREVIEW:BOOL=ON" in cache,
                "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                "sha256": {name: hashlib.sha256((build / src).read_bytes()).hexdigest()
                           for name, src in sources.items()}}
    instructions = f"""Bambu Bot {version} - Desk-bot hardware DEMO

ESP32-S3, flash 16 MB, octal PSRAM, ST7789 and I2S pinout from config.h.
This is simulated printer data. Servo outputs are disabled.
UI: short BOOT toggles chat; hold BOOT toggles black/green vs green/white theme.
Theme is saved to NVS. The compact LCD intentionally hides the DEMO label.
Head-only 3D UI renders at 140x120, scaled to 280x240; telemetry icons/text are native resolution.
Coffee on 280x240 uses native 167x129 pre-rendered RGB565 frames, 50 ms target (20 FPS).
Other 3D scenes retain the 100 ms CPU renderer. When enabled, the coffee preview profile
overrides idle/printing visuals; conversation and printer alerts take priority.
Actual LCD FPS and audio/MQTT performance of this experimental clip are unverified.

Install esptool 4.x in your Python environment or activate ESP-IDF 5.5.4.
Unzip all files in one directory. Replace PORT with the USB-UART device path:

python -m esptool --chip esp32s3 --port PORT --baud 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin 0x20000 bambu_bot.bin 0x800000 generated_assets.bin

Do not use native USB pins GPIO19/20 while connected to the I2S speaker.
Use the USB-UART/programming wiring of your existing Desk-bot.
Existing NVS is not included or intentionally erased by this command.
Check source README.md for Wi-Fi, AI activation, LAN configuration and limitations.
"""
    target = ROOT / f"dist/bambu-bot-deskbot-demo-v{version}.zip"
    target.parent.mkdir(exist_ok=True)
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, src in sources.items():
            archive.write(build / src, name)
        archive.writestr("manifest.json", json.dumps(manifest, indent=2) + "\n")
        archive.writestr("FLASHING.txt", instructions)
    with zipfile.ZipFile(target) as archive:
        assert archive.testzip() is None
    print(target)


if __name__ == "__main__":
    main()
