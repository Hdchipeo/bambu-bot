#!/usr/bin/env python3
"""Keep matching, PRIVATE ELF symbols by SHA256 for later panic decoding."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import os
import time
import re


def archive(build):
    build=Path(build);elf=build/'bambu_bot.elf'
    if not elf.is_file():raise ValueError('Build the app ELF first')
    with elf.open('rb') as stream:
        digest=hashlib.file_digest(stream,'sha256').hexdigest()
    directory=build/'diagnostics';directory.mkdir(exist_ok=True)
    target=directory/(digest+'.elf')
    if not target.exists():
        with tempfile.NamedTemporaryFile(dir=directory,delete=False) as tmp:
            staged=Path(tmp.name)
        try:shutil.copyfile(elf,staged);os.replace(staged,target)
        finally:staged.unlink(missing_ok=True)
    description=json.loads((build/'project_description.json').read_text())
    metadata=dict(elf_sha256=digest,project_version=description['project_version'],private_symbols=True,archived_at_ns=time.time_ns())
    (directory/(digest+'.json')).write_text(json.dumps(metadata,indent=2)+'\n')
    # Keep the current ELF and two earlier builds; these may contain local credentials.
    def archived_at(path):
        metadata_file=path.with_suffix('.json')
        if metadata_file.exists():return json.loads(metadata_file.read_text()).get('archived_at_ns',path.stat().st_mtime_ns)
        return path.stat().st_mtime_ns
    others=sorted((p for p in directory.glob('*.elf') if p!=target and re.fullmatch(r'[0-9a-f]{64}',p.stem)),key=archived_at,reverse=True)
    for path in others[2:]:
        path.unlink();path.with_suffix('.json').unlink(missing_ok=True)
    print(f'Private crash symbols: diagnostics/{digest}.elf (v{metadata["project_version"]})')
    return target

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('build',type=Path);args=parser.parse_args()
    archive(args.build)
if __name__=='__main__':main()
