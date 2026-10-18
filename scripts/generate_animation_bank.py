#!/usr/bin/env python3
"""Build lossless dual-theme palette/zlib animation assets from --export-animations streams.
Requires numpy for offline generation only. No firmware/network configuration is read.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SCENES=['Game time','Coffee break','Music lover','Curious explorer','Sleepy afternoon','A visiting fly',
        'Printer check','Print complete!','Mini racer','Bubble tea','Just chilling','Printer alert',
        'Robot disco','Plant parent','Paper pilot','Little magician','Stretch break','Hello, maker!']
EMOTIONS=['Idle','Listening','Thinking','Speaking','Printing','Happy','Completed','Curious','Confused',
          'Sleepy','Sad','Warning','Error','Offline']
HEADER=32
ENTRY=40

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('sources',type=Path)
    args=parser.parse_args()
    clips=[]
    max_palette_colors=0
    for clip_id,name in enumerate(SCENES+EMOTIONS):
        n=150 if clip_id==1 else 88
        themes=[np.fromfile(args.sources/f'clip-{clip_id:02d}-{theme}.rgb565',dtype='<u2').reshape(n,240,280)
                for theme in ('black','green')]
        mask=np.zeros((240,280),dtype=bool)
        for frames in themes:
            mask|=np.any(frames!=frames[0,0,0],axis=0)
        ys,xs=np.nonzero(mask)
        x0,y0=max(0,int(xs.min())-2),max(0,int(ys.min())-2)
        x1,y1=min(280,int(xs.max())+3),min(240,int(ys.max())+3)
        w,h=x1-x0,y1-y0
        scale=min(1.,280/w,152/h)
        dw,dh=max(1,int(w*scale)),max(1,int(h*scale))
        x=max(0,min(280-dw,round((x0+x1-dw)/2)))
        y=max(44,min(196-dh,round((y0+y1-dh)/2)))
        # One fixed fit for every frame/theme prevents pumping or jitter at crop changes.
        xi=np.minimum(w-1,((np.arange(dw)+.5)*w/dw).astype(int))+x0
        yi=np.minimum(h-1,((np.arange(dh)+.5)*h/dh).astype(int))+y0
        encoded=[]
        for frame_id in range(n):
            raw=[np.ascontiguousarray(frames[frame_id][yi[:,None],xi[None,:]],dtype='<u2')
                 for frames in themes]
            # Identical geometry shares indices; each palette entry preserves both
            # theme colors exactly. No quantization or temporal dependencies.
            pairs=raw[0].astype(np.uint32).ravel() | (raw[1].astype(np.uint32).ravel()<<16)
            colors,indices=np.unique(pairs,return_inverse=True)
            max_palette_colors=max(max_palette_colors,len(colors))
            assert 0<len(colors)<=1024, 'Dual-theme palette exceeds decoder limit'
            packed=(struct.pack('<H',len(colors))+colors.astype('<u4').tobytes()
                    +indices.astype('u1' if len(colors)<=256 else '<u2').tobytes())
            compressed=zlib.compress(packed,9)
            assert zlib.decompress(compressed)==packed
            for theme in (0,1):
                restored=((colors[indices]>>(16*theme))&65535).astype('<u2').tobytes()
                assert restored==raw[theme].tobytes()
            encoded.append(compressed)
        for theme in (0,1):
            clips.append(dict(id=clip_id,name=name,kind='scene' if clip_id<18 else 'emotion',theme=theme,
                              x=x,y=y,w=dw,h=dh,frames=n,intro=24,frame_ms=50,source_scale=scale,data=encoded))
        print(f'Packed {clip_id+1}/32 {name}',flush=True)
    base=HEADER+len(clips)*ENTRY+sum(c['frames']*8 for c in clips)
    payload=bytearray()
    unique={}
    table=bytearray()
    directory=bytearray()
    manifest=[]
    for c in clips:
        refs=bytearray()
        for frame in c['data']:
            if frame not in unique:
                unique[frame]=(base+len(payload),len(frame))
                payload.extend(frame)
            refs.extend(struct.pack('<II',*unique[frame]))
        offset=HEADER+len(clips)*ENTRY+len(table)
        directory.extend(struct.pack('<10I',c['id'],c['theme'],c['x'],c['y'],c['w'],c['h'],c['frames'],c['intro'],50,offset))
        table.extend(refs)
        manifest.append({k:v for k,v in c.items() if k!='data'})
    max_pixels=max(c['w']*c['h'] for c in clips)
    bank=struct.pack('<8s6I',b'BBANIM03',3,len(clips),base+len(payload),max_pixels,ENTRY,max_palette_colors)+directory+table+payload
    assert len(bank)==base+len(payload)
    assert len(bank)<7*1024*1024,'Animation bank leaves insufficient room for speech assets'
    dest=ROOT/'ui/animation_assets/animations.bba'
    dest.parent.mkdir(exist_ok=True)
    dest.write_bytes(bank)
    summary=dict(version=3,scenes=18,emotions=14,themes=2,bytes=len(bank),max_pixels=max_pixels,
                 double_buffer_bytes=max_pixels*4,decode_scratch_bytes=max_pixels*(1 if max_palette_colors<=256 else 2)+2+4*max_palette_colors,
                 max_palette_colors=max_palette_colors,unique_frames=len(unique),stored_frame_references=sum(c['frames'] for c in clips),
                 sha256=hashlib.sha256(bank).hexdigest(),codec='independent zlib dual-theme lossless RGB565 palette',clips=manifest)
    (ROOT/'ui/assets/animation_bank.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k!='clips'},indent=2))

if __name__=='__main__':main()
