#!/usr/bin/env python3
"""Package the credential-free default build for manual flashing."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile
from verify_animation_assets import verify_assets

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
    animation_bank = verify_assets(build / "generated_assets.bin")
    manifest = {"animation_bank": animation_bank,"project": "bambu-bot", "version": version, "profile": "deskbot-demo",
                "forced_demo": forced_demo,
                "coffee_preview": "BAMBU_COFFEE_PREVIEW:BOOL=ON" in cache,
                "printer_preview": "BAMBU_PRINTER_PREVIEW:BOOL=ON" in cache,
                "animation_preview": "BAMBU_ANIMATION_PREVIEW:BOOL=ON" in cache,
                "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                "sha256": {name: hashlib.sha256((build / src).read_bytes()).hexdigest()
                           for name, src in sources.items()}}
    instructions = f"""Bambu Bot {version} - Desk-bot hardware DEMO

ESP32-S3, flash 16 MB, octal PSRAM, ST7789 and I2S pinout from config.h.
This is simulated printer data. Servo outputs are disabled.
UI: short BOOT toggles chat; hold BOOT toggles black/green vs green/white theme.
Theme is saved to NVS. The compact LCD intentionally hides the DEMO label.
All 18 3D scenes and 14 head emotions use native RGB565/zlib animation clips on 280x240.
The 5.28 MiB animation bank is in the existing assets partition, separate from app code.
Telemetry icons/text remain independent native LVGL objects. Animation target: 50 ms (20 FPS).
The full demo-preview profile cycles scenes and heads; its printer telemetry is simulated.
Normal firmware retains automatic selection and prioritizes conversations and printer alerts.
Missing/corrupt animation assets fall back to the CPU mesh renderer.
Actual LCD FPS and simultaneous audio/MQTT performance of this version are unverified.

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
