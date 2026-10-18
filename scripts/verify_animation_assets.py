#!/usr/bin/env python3
"""Verify the animation bank and its inclusion in the firmware assets image."""
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT=Path(__file__).resolve().parents[1]

def verify_bank(data):
    manifest=json.loads((ROOT/'ui/assets/animation_bank.json').read_text())
    digest=hashlib.sha256(data).hexdigest()
    if len(data)!=manifest['bytes'] or digest!=manifest['sha256'] or data[:8]!=b'BBANIM02':
        raise ValueError('Animation bank differs from the source manifest; regenerate/rebuild assets')
    return {key:manifest[key] for key in ('version','bytes','sha256','scenes','emotions','themes','double_buffer_bytes')}

def verify_assets(path):
    data=Path(path).read_bytes()
    if len(data)>8*1024*1024 or len(data)<12:
        raise ValueError('Assets image does not fit the existing 8 MiB partition')
    count,checksum,length=struct.unpack_from('<III',data)
    if count>4096 or 12+count*44>len(data) or length!=len(data)-12 or (sum(data[12:])&0xFFFF)!=checksum:
        raise ValueError('Invalid assets image header/table/checksum')
    for i in range(count):
        name,size,offset,_,_=struct.unpack_from('<32sIIHH',data,12+i*44)
        if name.rstrip(b'\0')==b'animations.bba':
            start=12+count*44+offset
            if start+2+size>len(data) or data[start:start+2]!=b'ZZ':
                raise ValueError('Animation asset range/magic is invalid')
            result=verify_bank(data[start+2:start+2+size])
            result['assets_image_bytes']=len(data)
            return result
    raise ValueError('animations.bba missing from firmware assets; rebuild default assets')

if __name__=='__main__':
    print(json.dumps(verify_assets(sys.argv[1]),indent=2))
