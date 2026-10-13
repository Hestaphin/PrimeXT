#!/usr/bin/env python3
"""Extract reference bones from a matching Studio v10 MDL for native captures.
This never converts an obsolete .phys or invents bind rotations from bounds.
"""
from pathlib import Path
import argparse, hashlib, json, math, struct
def extract(data):
    if len(data)<244 or data[:4]!=b'IDST' or struct.unpack_from('<i',data,4)[0]!=10:
        raise ValueError('Expected a complete IDST Studio v10 model header')
    length=struct.unpack_from('<i',data,72)[0]
    count,offset=struct.unpack_from('<ii',data,140)
    if length<244 or length>len(data) or not 1<=count<=1024 or offset<244 or offset+count*112>length:
        raise ValueError('Invalid model length or bone table bounds')
    bones=[];names=set()
    for i in range(count):
        at=offset+i*112;raw=data[at:at+32].split(b'\0',1)[0]
        try:name=raw.decode('ascii')
        except UnicodeDecodeError:raise ValueError('Native capture requires ASCII Studio bone names')
        parent=struct.unpack_from('<i',data,at+32)[0];values=struct.unpack_from('<6f',data,at+64)
        if not name or any(ord(c)<32 for c in name) or name.lower() in names or parent< -1 or parent>=count or parent==i or not all(math.isfinite(v) and abs(v)<1e8 for v in values):
            raise ValueError('Invalid/duplicate bone name, parent or reference value')
        names.add(name.lower());bones.append((name,parent,values))
    for i in range(count):
        seen=set();current=i
        while current>=0:
            if current in seen:raise ValueError('Cyclic Studio reference skeleton')
            seen.add(current);current=bones[current][1]
    return bones
def encode(bones):
    return 'studio_bind_v1\n'+''.join(json.dumps(n)+' '+str(p)+' '+' '.join(format(v,'.9g') for v in values)+'\n' for n,p,values in bones)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--model',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--json',type=Path);a=p.parse_args()
    data=a.model.read_bytes()
    try:bones=extract(data)
    except ValueError as e:p.error(str(e))
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(encode(bones),encoding='utf-8')
    manifest={'model':str(a.model.resolve()),'model_sha256':hashlib.sha256(data).hexdigest(),'reference_bones':len(bones),'bind_sha256':hashlib.sha256(a.out.read_bytes()).hexdigest(),'phys_conversion':False}
    if a.json:a.json.parent.mkdir(parents=True,exist_ok=True);a.json.write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
