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
    if len(data)!=manifest['bytes'] or digest!=manifest['sha256'] or data[:8]!=b'BBANIM03':
        raise ValueError('Animation bank differs from the source manifest; regenerate/rebuild assets')
    return {key:manifest[key] for key in ('version','bytes','sha256','scenes','emotions','themes','double_buffer_bytes')}

def verify_assets(path):
    data=Path(path).read_bytes()
    if len(data)>8*1024*1024 or len(data)<12:
        raise ValueError(f'Assets image is {len(data):,} bytes; the 8 MiB partition allows {8*1024*1024:,} bytes '
                         f'(excess: {max(0,len(data)-8*1024*1024):,}). Reduce model/animation assets and rebuild; do not bypass this check.')
    count,checksum,length=struct.unpack_from('<III',data)
    if count>4096 or 12+count*44>len(data) or length!=len(data)-12 or (sum(data[12:])&0xFFFF)!=checksum:
        raise ValueError('Invalid assets image header/table/checksum')
    assets={}
    for i in range(count):
        name,size,offset,_,_=struct.unpack_from('<32sIIHH',data,12+i*44)
        start=12+count*44+offset
        if start+2+size>len(data) or data[start:start+2]!=b'ZZ':
            raise ValueError('Asset range/magic is invalid')
        assets[name.rstrip(b'\0').decode('utf-8')]=data[start+2:start+2+size]
    if 'animations.bba' not in assets:
        raise ValueError('animations.bba missing from firmware assets; rebuild default assets')
    result=verify_bank(assets['animations.bba'])
    prompts=json.loads((ROOT/'ui/assets/notification_prompts.json').read_text())
    for info in prompts.values():
        voice=assets.get(info['file'],b'')
        if len(voice)!=info['bytes'] or hashlib.sha256(voice).hexdigest()!=info['sha256']:
            raise ValueError('Notification voice asset missing or stale; rebuild default assets')
        magic,rate,frame_ms,packets,total=struct.unpack_from('<8s4I',voice)
        if magic!=b'BBVOICE1' or rate!=16000 or frame_ms!=60 or packets!=info['packets'] or not 50<packets<=512 or total!=len(voice):
            raise ValueError('Invalid notification Opus header')
        pos=24
        for _ in range(packets):
            if pos+2>len(voice):raise ValueError('Truncated notification Opus length')
            length=struct.unpack_from('<H',voice,pos)[0];pos+=2
            if not 0<length<=1275 or pos+length>len(voice):raise ValueError('Invalid notification Opus packet')
            pos+=length
        if pos!=len(voice):raise ValueError('Invalid notification Opus trailing data')
    result['notification_audio_bytes']=sum(i['bytes'] for i in prompts.values())
    result['assets_image_bytes']=len(data)
    return result

if __name__=='__main__':
    print(json.dumps(verify_assets(sys.argv[1]),indent=2))
