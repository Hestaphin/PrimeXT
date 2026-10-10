"""PrimeXT Weapon & Cartridge Editor: deterministic on-disk J.4/J.5 project backend.

No game process and no pxmv dependency. The native validator invokes the actual
C++ modular::Registry loader; Python checks provide immediate editor feedback.
"""
from __future__ import annotations
from collections import OrderedDict
from dataclasses import dataclass, field
from pathlib import Path
import math
import os
import re
import shutil
import subprocess
import tempfile

ID = re.compile(r"^[A-Za-z0-9_-]{1,63}$")
ASSET = re.compile(r"^[A-Za-z0-9_./-]{1,240}$")
CARTRIDGE_DEFAULTS = OrderedDict([
    ('version','2'), ('id','new_cartridge'), ('chamber','9x19'),
    ('projectile','fmj'), ('mass_grams','8.04'), ('diameter_mm','9.02'),
    ('drag_coefficient','0.30'), ('reference_velocity_mps','360'),
    ('projectile_count','1'), ('damage','20')])
WEAPON_DEFAULTS = OrderedDict([
    ('version','2'), ('id','new_weapon'), ('display_name','New Weapon'),
    ('runtime_id','32'), ('ammo_name','new_cartridge'), ('default_cartridge','new_cartridge'),
    ('clip_size','15'), ('default_ammo','30'), ('max_carry','120'),
    ('slot','1'), ('position','0'), ('weight','10'), ('flags','0'),
    ('view_model','models/v_9mmhandgun.mdl'), ('player_model','models/p_9mmhandgun.mdl'),
    ('world_model','models/w_9mmhandgun.mdl'),
    ('fire_interval','0.2'), ('spread','0.01'), ('reload_seconds','1.5'),
    ('range','8192'), ('bullet_type','1'), ('npc_usable','1'),
    ('fire_animation','3'),('draw_animation','5'),('reload_animation','6'),
    ('primary_behavior','ballistic'), ('primary_mode','semi'), ('primary_interval','0.2'),
    ('primary_spread','0.01'), ('primary_projectiles','1'), ('primary_ammo_cost','1'),
    ('primary_burst_count','3'),('primary_recoil_pitch','-2'),('primary_recoil_yaw','0'),
    ('primary_animation','3'),
    ('secondary_behavior','ammo_cycle'), ('secondary_mode','semi'),('secondary_ammo_cost','0'),
    ('muzzle_flash','true'), ('fire_underwater','true'), ('auto_reload','true'),
    ('idle_animation','-1'), ('idle_seconds','3'), ('animation_extension','onehanded')])
OPTIONAL_WEAPON = ('fire_sound','reload_sound','pickup_sound','sprite','fire_event',
    'primary_sound','primary_event','secondary_sound','secondary_event')

@dataclass
class Asset:
    kind: str
    fields: OrderedDict[str,str]
    accepted: OrderedDict[str,str] = field(default_factory=OrderedDict)
    @property
    def id(self) -> str: return self.fields['id']
    def serialize(self) -> str:
        data = ''.join(f'{k} = {v}\n' for k,v in self.fields.items() if v != '')
        if self.kind == 'weapon':
            data += ''.join(f'accept = {cid}:{velocity}\n' for cid,velocity in self.accepted.items())
        return data

def parse(text: str, kind: str) -> Asset:
    if len(text.encode('utf-8')) > 65536: raise ValueError('file exceeds 64 KiB')
    fields = OrderedDict()
    accepted = OrderedDict()
    for line_no, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line or line.startswith(('#',';')): continue
        if '=' not in line: raise ValueError(f'line {line_no}: expected key = value')
        key, value = (s.strip() for s in line.split('=',1))
        if not key or not value: raise ValueError(f'line {line_no}: empty key/value')
        if key == 'accept' and kind == 'weapon':
            if ':' not in value: raise ValueError(f'line {line_no}: expected cartridge_id:velocity')
            cid, vel = (s.strip() for s in value.split(':',1))
            if cid in accepted: raise ValueError(f'line {line_no}: duplicate compatible cartridge')
            accepted[cid] = vel
        else:
            if key in fields: raise ValueError(f'line {line_no}: duplicate field {key}')
            fields[key] = value
    return Asset(kind,fields,accepted)

def read_manifest(root: Path, kind: str) -> list[str]:
    manifest = root/'scripts'/(kind+'s')/'manifest.txt'
    if not manifest.exists(): return []
    ids=[]
    for line in manifest.read_text(encoding='utf-8').splitlines():
        name=line.strip()
        if not name or name.startswith(('#',';')): continue
        if not ID.fullmatch(name): raise ValueError(f'invalid manifest entry {name!r}')
        if name in ids: raise ValueError(f'duplicate {kind} {name}')
        ids.append(name)
    return ids

def positive_float(value: str, low: float, high: float, label: str) -> float:
    try: v=float(value)
    except (ValueError,TypeError): raise ValueError(f'{label} must be a number') from None
    if not math.isfinite(v) or v<low or v>high: raise ValueError(f'{label} outside [{low},{high}]')
    return v

def bounded_int(value: str, low: int, high: int, label: str) -> int:
    try:
        if not re.fullmatch(r'[+-]?[0-9]+',value): raise ValueError()
        v=int(value)
    except (ValueError,TypeError): raise ValueError(f'{label} must be an integer') from None
    if v<low or v>high: raise ValueError(f'{label} outside [{low},{high}]')
    return v

def validate(weapons: dict[str,Asset], cartridges: dict[str,Asset]) -> list[str]:
    """Immediate checks; native CLI is authoritative for the entire schema."""
    errors=[]; runtime={}
    ammo_ids={}
    for name in cartridges:
        previous=ammo_ids.setdefault(name.casefold(),name)
        if previous!=name:
            errors.append(f'cartridge {name}: case-insensitive ammo name conflicts with {previous}')
    if len(weapons)>31: errors.append('At most 31 modular weapons (IDs 32..62)')
    if len(cartridges)>31: errors.append('Legacy ammo registry has 32 slots; leave a margin for base game ammo')
    for kind, collection in [('cartridge',cartridges),('weapon',weapons)]:
        for name,a in collection.items():
            prefix=f'{kind} {name}'
            if not ID.fullmatch(name) or name != a.fields.get('id'): errors.append(f'{prefix}: invalid ID or filename mismatch')
            for k,v in a.fields.items():
                if '\n' in k+v or '\r' in k+v or '\x00' in k+v or not v.strip():
                    errors.append(f'{prefix}: illegal value for {k}')
            for k in ('view_model','player_model','world_model','fire_sound','reload_sound','pickup_sound','sprite','fire_event','primary_sound','secondary_sound','primary_event','secondary_event'):
                if k in a.fields and a.fields[k]:
                    v=a.fields[k]
                    if not ASSET.fullmatch(v) or '..' in v or v.startswith('/'):
                        errors.append(f'{prefix}: unsafe asset path {k}')
            try:
                if kind=='cartridge':
                    bounded_int(a.fields.get('version',''),1,2,'version')
                    positive_float(a.fields.get('mass_grams',''),.001,100000,'mass_grams')
                    positive_float(a.fields.get('diameter_mm',''),.001,10000,'diameter_mm')
                    positive_float(a.fields.get('drag_coefficient',''),0,20,'drag')
                    positive_float(a.fields.get('reference_velocity_mps',''),.01,10000,'reference_velocity')
                    bounded_int(a.fields.get('projectile_count',''),1,256,'projectile_count')
                    if a.fields.get('version')=='2': positive_float(a.fields.get('damage',''),.01,1000,'damage')
                else:
                    rid=bounded_int(a.fields.get('runtime_id',''),32,62,'runtime_id')
                    if rid in runtime: errors.append(f'{prefix}: runtime ID {rid} duplicates {runtime[rid]}')
                    runtime[rid]=name
                    bounded_int(a.fields.get('slot',''),0,4,'slot')
                    bounded_int(a.fields.get('clip_size',''),1,255,'clip_size')
                    bounded_int(a.fields.get('position',''),0,9,'position')
                    bounded_int(a.fields.get('max_carry',''),1,999,'max_carry')
                    default=a.fields.get('default_cartridge')
                    if default not in a.accepted: errors.append(f'{prefix}: default cartridge not accepted')
                    if not a.accepted: errors.append(f'{prefix}: at least one compatible cartridge is needed')
                    for cid, vel in a.accepted.items():
                        if cid not in cartridges: errors.append(f'{prefix}: unknown cartridge {cid}')
                        positive_float(vel,.01,10000,'muzzle velocity')
            except ValueError as e: errors.append(f'{prefix}: {e}')
    return errors

class Project:
    def __init__(self, root: Path):
        self.root=Path(root).resolve()
        self.weapons: dict[str,Asset]={}; self.cartridges: dict[str,Asset]={}

    def load(self) -> None:
        """Fail closed on invalid manifests/assets; never scan arbitrary assets."""
        new={}
        for kind in ('cartridge','weapon'):
            assets={}
            for name in read_manifest(self.root,kind):
                path=self.root/'scripts'/(kind+'s')/(name+'.'+kind)
                a=parse(path.read_text(encoding='utf-8'),kind)
                if a.id != name: raise ValueError(f'{path}: manifest ID differs from file ID')
                assets[name]=a
            new[kind]=assets
        self.cartridges=new['cartridge']; self.weapons=new['weapon']

    def add(self,kind:str,name:str,copy: str|None =None) -> Asset:
        if not ID.fullmatch(name): raise ValueError('asset ID must use letters, numbers, _ or - (max 63)')
        coll=self.weapons if kind=='weapon' else self.cartridges
        if name in coll: raise ValueError('asset ID already exists')
        template=coll[copy] if copy else None
        if template:
            fields=OrderedDict(template.fields); accepted=OrderedDict(template.accepted)
        else:
            fields=OrderedDict(WEAPON_DEFAULTS if kind=='weapon' else CARTRIDGE_DEFAULTS)
            accepted=OrderedDict()
            if kind=='weapon':
                fields['runtime_id']=next((str(i) for i in range(32,63) if all(w.fields.get('runtime_id')!=str(i) for w in coll.values())), '-1')
                if self.cartridges:
                    cid=next(iter(self.cartridges))
                    fields['ammo_name']=fields['default_cartridge']=cid
                    accepted[cid]=self.cartridges[cid].fields.get('reference_velocity_mps','360')
        fields['id']=name
        if kind=='weapon' and not copy: fields['display_name']=name.replace('_',' ').title()
        a=Asset(kind,fields,accepted);coll[name]=a
        return a

    def remove(self,kind:str,name:str) -> None:
        if kind=='cartridge':
            referencing=[w.id for w in self.weapons.values() if name in w.accepted]
            if referencing: raise ValueError('Cartridge used by '+', '.join(referencing))
        del (self.weapons if kind=='weapon' else self.cartridges)[name]

    def save(self, validator: Path|None=None) -> None:
        errs=validate(self.weapons,self.cartridges)
        if errs: raise ValueError('\n'.join(errs))
        self.root.mkdir(parents=True,exist_ok=True)
        # Copy unrelated files into stage; validate the intended full layout with
        # the same C++ registry used by server and client before touching disk.
        with tempfile.TemporaryDirectory(prefix='primext_weapon_stage_') as tmp:
            stage=Path(tmp)
            for kind, assets in [('cartridge',self.cartridges),('weapon',self.weapons)]:
                folder=stage/'scripts'/(kind+'s'); folder.mkdir(parents=True)
                for key,asset in assets.items():
                    (folder/(key+'.'+kind)).write_text(asset.serialize(),encoding='utf-8')
                (folder/'manifest.txt').write_text(''.join(k+'\n' for k in assets),encoding='utf-8')
            if validator:
                result=subprocess.run([str(validator),str(stage)],text=True,capture_output=True,timeout=10)
                if result.returncode: raise ValueError('Native registry rejected export:\n'+result.stdout+result.stderr)
            # Validate completed; replace entries and manifests with no unverified output.
            for kind,assets in [('cartridge',self.cartridges),('weapon',self.weapons)]:
                folder=self.root/'scripts'/(kind+'s');folder.mkdir(parents=True,exist_ok=True)
                existing=set(read_manifest(self.root,kind))
                for name in assets:
                    os.replace(stage/'scripts'/(kind+'s')/(name+'.'+kind),folder/(name+'.'+kind))
                os.replace(stage/'scripts'/(kind+'s')/'manifest.txt',folder/'manifest.txt')
                # Remove only previously manifest-owned files absent from new registry.
                for name in existing-set(assets):
                    (folder/(name+'.'+kind)).unlink(missing_ok=True)


def flight(cartridge: Asset, muzzle_velocity:float, max_seconds:float=2.0, dt:float=.005):
    """Non-authoritative trajectory preview mirroring game_shared/ballistic_flight.cpp."""
    mass=float(cartridge.fields['mass_grams'])/1000.0
    diameter=float(cartridge.fields['diameter_mm'])/1000.0
    cd=float(cartridge.fields['drag_coefficient'])
    area=math.pi*diameter*diameter*.25
    k=.5*1.225*cd*area/mass
    x=y=0.0;vx=muzzle_velocity;vy=0.0; points=[(0.,0.)]
    for i in range(int(max_seconds/dt)):
        old_vx,old_vy=vx,vy
        half_y=vy-9.80665*dt*.5
        drag=1/(1+k*math.hypot(vx,half_y)*dt)
        vx=vx*drag;vy=half_y*drag-9.80665*dt*.5
        x+=(old_vx+vx)*.5*dt; y+=(old_vy+vy)*.5*dt
        if i%10==0: points.append((x,y))
        if x>5000: break
    return points
