#!/usr/bin/env python3
"""Offline macOS Linh TTS -> mono 16 kHz / 60 ms Opus packets for XiaoZhi ASR.
No text injection, microphone recording or external TTS service is used.
"""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
PROMPTS={
    'start':'Máy in vừa bắt đầu in. Hãy đọc ngữ cảnh thông báo và trạng thái máy in bằng công cụ, rồi thông báo cho tôi tiến độ và thời gian còn lại. Nếu không có dữ liệu thì nói rõ chưa biết.',
    'finish':'Bản in vừa hoàn thành. Hãy đọc ngữ cảnh thông báo và trạng thái máy in bằng công cụ, rồi báo tin cho tôi. Chỉ xác nhận hoàn tất khi dữ liệu máy in xác nhận.',
    'error':'Máy in có cảnh báo mới. Hãy đọc ngữ cảnh thông báo, trạng thái máy in và chẩn đoán HMS bằng công cụ. Tra cứu Bambu Wiki để giải thích lỗi và cách khắc phục, kèm nguồn. Nếu chưa rõ thì không đoán.',
}

def opus_packets(blob):
    pos=0;pending=bytearray();packets=[]
    while pos<len(blob):
        if blob[pos:pos+4]!=b'OggS':raise ValueError('Invalid Ogg stream')
        n=blob[pos+26];segments=blob[pos+27:pos+27+n];cursor=pos+27+n
        for length in segments:
            pending.extend(blob[cursor:cursor+length]);cursor+=length
            if length<255:
                packet=bytes(pending);pending.clear()
                if not packet.startswith((b'OpusHead',b'OpusTags')):packets.append(packet)
        pos=cursor
    if pending or not packets or any(not p or len(p)>1275 for p in packets):raise ValueError('Invalid Opus packets')
    return packets

def main():
    metadata={}
    with tempfile.TemporaryDirectory(prefix='bambu-voice-') as tmp:
        for kind,text in PROMPTS.items():
            speech=Path(tmp)/f'{kind}.aiff';opus=Path(tmp)/f'{kind}.opus'
            subprocess.run(['say','-v','Linh','-r','175','-o',str(speech),text],check=True)
            subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-i',str(speech),'-ac','1','-ar','16000',
                            '-af','adelay=150,apad=pad_dur=0.6','-c:a','libopus','-application','voip','-b:a','20k',
                            '-frame_duration','60',str(opus)],check=True)
            pcm=subprocess.check_output(['ffmpeg','-hide_banner','-loglevel','error','-i',str(opus),'-f','s16le','-ac','1','-ar','16000','-'])
            import array
            samples=array.array('h',pcm)
            if len(samples)<3*16000 or max(abs(x) for x in samples)<500:
                raise ValueError('TTS produced silence/short audio; check macOS speech-service access')
            packets=opus_packets(opus.read_bytes())
            body=b''.join(struct.pack('<H',len(p))+p for p in packets)
            data=struct.pack('<8s4I',b'BBVOICE1',16000,60,len(packets),24+len(body))+body
            assert len(data)<=98304 and len(packets)<=512
            file=f'notify_{kind}.bbv';(ROOT/'ui/animation_assets'/file).write_bytes(data)
            metadata[kind]=dict(file=file,text=text,voice='macOS Linh vi_VN',sample_rate=16000,frame_ms=60,
                                packets=len(packets),duration_ms=len(packets)*60,bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
            print(kind,len(data),'bytes',len(packets)*60,'ms')
    (ROOT/'ui/assets/notification_prompts.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
