#!/usr/bin/env python3
"""Package the credential-free default build for manual flashing."""
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    if (ROOT / "main/bambu/local_config.h").exists():
        raise SystemExit("Refusing to package a build with local configuration. Remove it and rebuild the default demo first.")
    description = json.loads((ROOT / "build/project_description.json").read_text())
    version = description["project_version"]
    sources = {
        "bootloader.bin": "build/bootloader/bootloader.bin",
        "partition-table.bin": "build/partition_table/partition-table.bin",
        "ota_data_initial.bin": "build/ota_data_initial.bin",
        "bambu_bot.bin": "build/bambu_bot.bin",
        "generated_assets.bin": "build/generated_assets.bin",
    }
    manifest = {"project": "bambu-bot", "version": version, "profile": "deskbot-demo",
                "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                "sha256": {name: hashlib.sha256((ROOT / src).read_bytes()).hexdigest()
                           for name, src in sources.items()}}
    instructions = f"""Bambu Bot {version} - Desk-bot hardware DEMO

ESP32-S3, flash 16 MB, octal PSRAM, ST7789 and I2S pinout from config.h.
This is simulated printer data. Servo outputs are disabled.
UI: short BOOT toggles chat; hold BOOT toggles black/green vs green/white theme.
Theme is saved to NVS. The compact LCD intentionally hides the DEMO label.
Head-only 3D UI renders at 140x120, scaled to 280x240; telemetry icons/text are native resolution.
The 100 ms animation timer is a target; hardware FPS/audio/MQTT performance is unverified.
Firmware was compiled; physical hardware has not been verified.

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
            archive.write(ROOT / src, name)
        archive.writestr("manifest.json", json.dumps(manifest, indent=2) + "\n")
        archive.writestr("FLASHING.txt", instructions)
    with zipfile.ZipFile(target) as archive:
        assert archive.testzip() is None
    print(target)


if __name__ == "__main__":
    main()
