#!/usr/bin/env python3
"""Run the Mac/LAN retrieval service using the ignored Bambu Bot local config."""
import json
from pathlib import Path
from urllib.parse import urlparse
from http.server import ThreadingHTTPServer
from knowledge import Knowledge,handler

def main():
    root=Path(__file__).resolve().parents[1]
    config=json.loads((root/'bambu.local.json').read_text())
    token=config.get('wiki_token','');url=urlparse(config.get('wiki_url',''))
    if not token or not url.hostname:raise SystemExit('Configure wiki_url and wiki_token in bambu.local.json first')
    if url.scheme!='http':raise SystemExit('run_local serves LAN HTTP; use a TLS reverse proxy for HTTPS')
    server=ThreadingHTTPServer(('0.0.0.0',url.port or (443 if url.scheme=='https' else 80)),handler(Knowledge(),token))
    server.daemon_threads=True
    print(f'Bambu Wiki retrieval listening on LAN port {server.server_port}; bearer auth enabled',flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()
if __name__=='__main__':main()
