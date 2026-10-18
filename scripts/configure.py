#!/usr/bin/env python3
"""Generate ignored local configuration without putting secrets in CMake arguments."""
import argparse
import ipaddress
import json
from pathlib import Path
import re
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[1]


def generate(data, source_dir):
    allowed = {"demo", "host", "serial", "access_code", "ca_file", "ota_url", "voice_notifications", "wiki_url", "wiki_token"}
    if set(data) - allowed:
        raise ValueError("Unknown configuration keys")
    demo = data.get("demo", True)
    if type(demo) is not bool:
        raise ValueError("demo must be true or false")
    host, serial, access = (data.get(k, "") for k in ("host", "serial", "access_code"))
    if not all(isinstance(v, str) for v in (host, serial, access)):
        raise ValueError("Printer settings must be strings")
    cert = ""
    if not demo:
        ipaddress.IPv4Address(host)
        if not re.fullmatch(r"[A-Za-z0-9]{5,40}", serial):
            raise ValueError("Invalid printer serial")
        if not re.fullmatch(r"[A-Za-z0-9]{8}", access):
            raise ValueError("Expected the printer's 8-character LAN access code")
        ca_file = Path(data.get("ca_file", ""))
        if not ca_file.is_absolute():
            ca_file = source_dir / ca_file
        cert = ca_file.read_text()
        if "-----BEGIN CERTIFICATE-----" not in cert or len(cert) > 32768:
            raise ValueError("A valid local printer CA PEM file is required")
    ota = data.get("ota_url", "")
    if not isinstance(ota, str):
        raise ValueError("ota_url must be a string")
    if ota:
        parsed = urlparse(ota)
        if parsed.scheme not in ("http", "https") or not parsed.hostname or parsed.username or parsed.password:
            raise ValueError("Invalid server OTA discovery URL")
    voice = data.get("voice_notifications", True)
    if type(voice) is not bool:
        raise ValueError("voice_notifications must be true or false")
    wiki = data.get("wiki_url", "")
    token = data.get("wiki_token", "")
    if not isinstance(wiki,str) or not isinstance(token,str) or len(token)>256 or any(ord(c)<32 or ord(c)>126 for c in token):
        raise ValueError("Invalid Wiki backend settings")
    wiki=wiki.rstrip("/")
    if wiki:
        parsed=urlparse(wiki)
        if parsed.scheme not in ("http","https") or not parsed.hostname or parsed.username or parsed.password or parsed.query or parsed.fragment or parsed.path not in ("", "/"):
            raise ValueError("wiki_url must be a backend origin URL")
        if parsed.scheme=="http":
            if parsed.hostname!="localhost":
                address=ipaddress.ip_address(parsed.hostname)
                if not address.is_private or address.is_unspecified:raise ValueError("Use HTTPS outside the LAN")
            if not token:raise ValueError("wiki_token required for HTTP LAN backend")
    values = {"BAMBU_DEMO": "true" if demo else "false", "BAMBU_VOICE_NOTIFICATIONS": "true" if voice else "false"}
    for key, value in (("HOST", host), ("SERIAL", serial), ("ACCESS_CODE", access),
                       ("TLS_CA_PEM", cert), ("OTA_URL", ota), ("WIKI_URL", wiki), ("WIKI_TOKEN", token)):
        values["BAMBU_" + key] = json.dumps(value, ensure_ascii=True)
    return "#pragma once\n// Local credentials: do not commit or share firmware built with these.\n" + "".join(
        f"#define {key} {value}\n" for key, value in values.items())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path)
    args = parser.parse_args()
    try:
        result = generate(json.loads(args.config.read_text()), args.config.resolve().parent)
    except (ValueError, OSError, TypeError) as exc:
        # Do not echo input values or JSON parse errors that may contain secrets.
        parser.exit(2, "Invalid configuration; check field types, printer settings and CA file.\n")
    target = ROOT / "main/bambu/local_config.h"
    target.write_text(result)
    target.chmod(0o600)
    print("Local configuration generated. Rebuild firmware; no credentials printed.")


if __name__ == "__main__":
    main()
