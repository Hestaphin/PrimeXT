#!/usr/bin/env python3
"""Capture real SDK drops of a freshly generated v5 rig and matching MDL.
Exit success means captures completed, not that solver/mesh/game acceptance passed.
"""
from pathlib import Path
import argparse, csv, hashlib, itertools, json, math, subprocess, sys
from prepare_f3_drop_input import extract, encode
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def metrics(p):
    rows=list(csv.DictReader(p.open()));result={'samples':len(rows)}
    if not rows:raise ValueError('Native capture contains no samples')
    keys=['root_speed','max_linear','max_angular','max_relative_angular','anchor_error','root_step_delta','penetration_sampled']
    for key in keys:
        values=[float(row[key]) for row in rows]
        if not all(math.isfinite(v) and v>=0 for v in values):raise ValueError('Invalid native sample: '+key)
        result[key+'_peak']=max(values)
        tail=[float(row[key]) for row in rows if 10<=float(row['sim_s'])<15]
        result[key+'_settled_tail_peak']=max(tail) if tail else None
    sleeps=[float(r['sim_s']) for r in rows if int(r['active'])==0 and not int(r['impulse_applied'])]
    result['first_all_sleep_s']=min(sleeps) if sleeps else None
    result['active_after_impulse']=any(int(r['active'])>0 for r in rows if int(r['impulse_applied']))
    result['wake_events']=int(rows[-1]['wake_events']);result['sleep_events']=int(rows[-1]['sleep_events'])
    return result
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True);p.add_argument('--asset',type=Path,required=True);p.add_argument('--model',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--scales',type=float,nargs='+',default=[1.]);a=p.parse_args()
    if not all(math.isfinite(s) and 0<s<=100 for s in a.scales):p.error('scales must be finite and in (0,100]')
    binary=a.binary.resolve();asset=a.asset.resolve();model=a.model.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
    try:bones=extract(model.read_bytes())
    except ValueError as e:p.error(str(e))
    bind=a.out/'reference.bind';bind.write_text(encode(bones))
    manifest={'model_sha256':digest(model),'asset_sha256':digest(asset),'native_binary_sha256':digest(binary),'bind_sha256':digest(bind),'cases':[],'physical_quality':'PENDING_REVIEW','mesh_game_fps':'NOT_RUN','contact_penetration':'maximum of up to 64 reported points per pair; scene contacts exclude CCD'}
    failed=False
    for scale,angle,step,mode in itertools.product(a.scales,[0,90,180,270],[.01,.02],['off','authored']):
        name=f'scale{scale:g}_angle{angle}_step{step:g}_{mode}';trace=a.out/(name+'.csv');log=a.out/(name+'.log');cmd=[str(binary),str(asset),str(bind),str(trace),str(angle),str(step),str(scale),mode]
        run=subprocess.run(cmd,text=True,capture_output=True);log.write_text(run.stdout+run.stderr);case={'name':name,'command':cmd,'exit_code':run.returncode,'trace':trace.name,'log':log.name}
        if run.returncode==0:
            try:case['measurements']=metrics(trace);case['trace_sha256']=digest(trace)
            except (ValueError,KeyError) as e:case['capture_error']=str(e);failed=True
        else:failed=True
        manifest['cases'].append(case);(a.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        print(('CAPTURED ' if run.returncode==0 else 'FAILED ')+name,flush=True)
    print('Review anchor growth, landing jitter, contacts, sleep and wake. Finite capture is not physical acceptance.');return 1 if failed else 0
if __name__=='__main__':raise SystemExit(main())
