"""Standalone Tk editor; deliberately does not import pxmv or game UI code."""
from __future__ import annotations
import copy
import math
from pathlib import Path
import struct
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, simpledialog
from project import Project, Asset, WEAPON_DEFAULTS, CARTRIDGE_DEFAULTS, OPTIONAL_WEAPON, validate, flight, positive_float
from validator_location import find_validator

WEAPON_SECTIONS = [
 ('Weapon', ['id','display_name','runtime_id','ammo_name','default_cartridge','clip_size',
             'default_ammo','max_carry','slot','position','weight','flags','npc_usable']),
 ('Firing', ['fire_interval','spread','reload_seconds','range','bullet_type',
             'muzzle_flash','fire_underwater','auto_reload','idle_animation','idle_seconds','animation_extension']),
 ('Primary', ['primary_behavior','primary_mode','primary_interval','primary_spread',
             'primary_projectiles','primary_ammo_cost','primary_burst_count',
             'primary_recoil_pitch','primary_recoil_yaw','primary_animation','primary_sound','primary_event']),
 ('Secondary', ['secondary_behavior','secondary_mode','secondary_interval','secondary_spread',
               'secondary_projectiles','secondary_ammo_cost','secondary_burst_count',
               'secondary_recoil_pitch','secondary_recoil_yaw','secondary_animation','secondary_sound','secondary_event']),
 ('Assets', ['view_model','player_model','world_model','fire_sound','reload_sound','pickup_sound',
             'sprite','fire_event','fire_animation','draw_animation','reload_animation'])]
CARTRIDGE_FIELDS = ['id','chamber','projectile','mass_grams','diameter_mm',
                    'drag_coefficient','reference_velocity_mps','projectile_count','damage']
ENUMS = {'primary_behavior':['ballistic','hitscan','ammo_cycle','none'],
         'secondary_behavior':['none','ammo_cycle','ballistic','hitscan'],
         'primary_mode':['auto','semi','burst','single'],
         'secondary_mode':['auto','semi','burst','single'],
         'npc_usable':['0','1'],'muzzle_flash':['true','false'],
         'fire_underwater':['true','false'],'auto_reload':['true','false']}

class ScrollForm(ttk.Frame):
    def __init__(self,parent):
        super().__init__(parent)
        self.canvas=tk.Canvas(self,highlightthickness=0,borderwidth=0)
        self.scroll=ttk.Scrollbar(self,orient='vertical',command=self.canvas.yview)
        self.inner=ttk.Frame(self.canvas,padding=12)
        self.win=self.canvas.create_window((0,0),window=self.inner,anchor='nw')
        self.canvas.configure(yscrollcommand=self.scroll.set)
        self.canvas.pack(side='left',fill='both',expand=True)
        self.scroll.pack(side='right',fill='y')
        self.inner.bind('<Configure>',lambda e:self.canvas.configure(scrollregion=self.canvas.bbox('all')))
        self.canvas.bind('<Configure>',lambda e:self.canvas.itemconfigure(self.win,width=e.width))

class ModelInfo:
    """Safe metadata-only GoldSrc Studio v10 viewer, no external render process."""
    def __init__(self,path:Path):
        with path.open('rb') as f:
            data=f.read(246)
            if len(data)<244: raise ValueError('truncated studio model')
            if data[:4] != b'IDST' or struct.unpack_from('<i',data,4)[0] != 10:
                raise ValueError('expected GoldSrc Studio IDST version 10')
            self.name=data[8:72].split(b'\0')[0].decode('latin1','replace')
            self.length=struct.unpack_from('<i',data,72)[0]
            if self.length<244 or self.length>path.stat().st_size: raise ValueError('invalid model length')
            self.mins=struct.unpack_from('<fff',data,88)
            self.maxs=struct.unpack_from('<fff',data,100)
            self.sequence_count=struct.unpack_from('<i',data,164)[0]
            seq_offset=struct.unpack_from('<i',data,168)[0]
            if self.sequence_count<0 or self.sequence_count>2048: raise ValueError('invalid sequence count')
            self.sequences=[]
            # mstudioseqdesc_t (GoldSrc Studio v10) has 176-byte stride.
            if seq_offset>=244 and seq_offset+self.sequence_count*176 <=self.length:
                f.seek(seq_offset)
                for _ in range(self.sequence_count):
                    d=f.read(176)
                    label=d[:32].split(b'\0')[0].decode('latin1','replace')
                    fps=struct.unpack_from('<f',d,32)[0]
                    frames=struct.unpack_from('<i',d,56)[0]
                    self.sequences.append((label,fps,frames))

class Editor(tk.Tk):
    def __init__(self,root:Path|None):
        super().__init__()
        self.title('PrimeXT Weapon & Cartridge Editor — J.5')
        self.geometry('1150x775');self.minsize(890,580)
        self.project=Project(root or Path.cwd())
        self.validator = find_validator(Path(__file__).parent / 'build')
        self.selection:tuple[str,str]|None=None
        self.widgets=[];self.variables={};self.undo=[];self.redo=[];self.dirty=False
        self._loading=False
        self._build()
        if root and root.exists(): self._open(root)
        else: self._refresh_tree()

    def _build(self):
        toolbar=ttk.Frame(self,padding=8);toolbar.pack(fill='x')
        for title,fn in [('Open Mod…',self.open_mod),('New Weapon',lambda:self.create('weapon')),
          ('New Cartridge',lambda:self.create('cartridge')),('Duplicate',self.duplicate),
          ('Delete',self.delete),('Undo',self.undo_action),('Redo',self.redo_action),
          ('Validate',self.check),('Save / Export',self.save)]:
            ttk.Button(toolbar,text=title,command=fn).pack(side='left',padx=2)
        self.path_label=ttk.Label(self,text=str(self.project.root),padding=(10,0))
        self.path_label.pack(fill='x')
        pane=ttk.Panedwindow(self,orient='horizontal');pane.pack(fill='both',expand=True,padx=9,pady=8)
        left=ttk.Frame(pane);pane.add(left,weight=1)
        self.tree=ttk.Treeview(left,show='tree',selectmode='browse')
        self.tree.pack(fill='both',expand=True)
        self.tree.bind('<<TreeviewSelect>>',self._selected)
        right=ttk.Frame(pane);pane.add(right,weight=4)
        self.tabs=ttk.Notebook(right);self.tabs.pack(fill='both',expand=True)
        self.formtabs={}
        for title in ('Weapon','Firing','Primary','Secondary','Assets','Cartridge'):
            frame=ScrollForm(self.tabs);self.tabs.add(frame,text=title);self.formtabs[title]=frame
        self.compat=ttk.Frame(self.tabs,padding=12);self.tabs.add(self.compat,text='Compatibility')
        self.preview=ttk.Frame(self.tabs,padding=12);self.tabs.add(self.preview,text='Ballistic Test')
        self.model=ttk.Frame(self.tabs,padding=12);self.tabs.add(self.model,text='Model / Animations')
        self.status=tk.StringVar(value='Choose a weapon or cartridge. Files are written only when Save / Export is clicked.')
        ttk.Label(self,textvariable=self.status,anchor='w',padding=8,relief='sunken').pack(fill='x')

    def _snapshot(self):
        return copy.deepcopy((self.project.weapons,self.project.cartridges))
    def _record(self):
        if self._loading: return
        self.undo.append(self._snapshot())
        if len(self.undo)>60: self.undo.pop(0)
        self.redo.clear();self.dirty=True
    def undo_action(self):
        if not self.undo: return
        self.redo.append(self._snapshot())
        self.project.weapons,self.project.cartridges=self.undo.pop();self._refresh_tree();self._show()
    def redo_action(self):
        if not self.redo: return
        self.undo.append(self._snapshot())
        self.project.weapons,self.project.cartridges=self.redo.pop();self._refresh_tree();self._show()
    def _open(self,root:Path):
        try:
            new=Project(root);new.load();self.project=new
            self.undo.clear();self.redo.clear();self.dirty=False;self.selection=None
            self.path_label.configure(text=str(root));self._refresh_tree();self._show()
            self.status.set(f'Opened {len(new.weapons)} weapons and {len(new.cartridges)} cartridges')
        except Exception as e: messagebox.showerror('Cannot open project',str(e))
    def open_mod(self):
        root=filedialog.askdirectory(initialdir=str(self.project.root),title='Choose mod directory')
        if not root: return
        if self.dirty and not messagebox.askyesno('Unsaved changes','Discard unsaved changes?'): return
        self._open(Path(root))
    def _refresh_tree(self):
        self._loading=True
        try:
            self.tree.delete(*self.tree.get_children())
            self.tree.insert('', 'end',iid='weapons',text=f'Weapons ({len(self.project.weapons)})',open=True)
            self.tree.insert('', 'end',iid='cartridges',text=f'Cartridges ({len(self.project.cartridges)})',open=True)
            for name in sorted(self.project.weapons): self.tree.insert('weapons','end',iid='weapon:'+name,text=name)
            for name in sorted(self.project.cartridges): self.tree.insert('cartridges','end',iid='cartridge:'+name,text=name)
            if self.selection:
                iid=f'{self.selection[0]}:{self.selection[1]}'
                if self.tree.exists(iid): self.tree.selection_set(iid)
        finally:self._loading=False
    def _selected(self,_=None):
        if self._loading:return
        sel=self.tree.selection()
        if not sel or ':' not in sel[0]:return
        kind,name=sel[0].split(':',1)
        self.selection=(kind,name);self._show()
    def _asset(self)->Asset|None:
        if not self.selection:return None
        kind,name=self.selection
        return (self.project.weapons if kind=='weapon' else self.project.cartridges).get(name)
    def create(self,kind):
        name=simpledialog.askstring('Create '+kind,'Asset ID (letters / digits / _ / -):',parent=self)
        if not name:return
        try:
            self._record();self.project.add(kind,name);self.selection=(kind,name)
            self._refresh_tree();self._show();self.dirty=True
        except Exception as e: self.undo.pop() if self.undo else None;messagebox.showerror('Cannot create asset',str(e))
    def duplicate(self):
        if not self.selection:return
        kind,name=self.selection
        new=simpledialog.askstring('Duplicate '+kind,'New asset ID:',initialvalue=name+'_copy',parent=self)
        if not new:return
        try:
            self._record();self.project.add(kind,new,copy=name);self.selection=(kind,new)
            self._refresh_tree();self._show()
        except Exception as e: self.undo.pop() if self.undo else None;messagebox.showerror('Cannot duplicate',str(e))
    def delete(self):
        if not self.selection:return
        kind,name=self.selection
        if not messagebox.askyesno('Delete asset',f'Delete {kind} {name}?') :return
        try:
            self._record();self.project.remove(kind,name);self.selection=None
            self._refresh_tree();self._show()
        except Exception as e: self.undo.pop() if self.undo else None;messagebox.showerror('Cannot delete',str(e))
    def _set(self,key,value):
        a=self._asset()
        if not a or key=='id':return
        if a.fields.get(key,'')==value:return
        self._record()
        if value=='': a.fields.pop(key,None)
        else:a.fields[key]=value
        self.status.set(f'Unsaved: {a.id} · {key} = {value}')
        if key in ('default_cartridge','mass_grams','drag_coefficient','diameter_mm','reference_velocity_mps'):
            self._draw_preview()
    def _show(self):
        a=self._asset()
        for title,form in self.formtabs.items():
            for child in form.inner.winfo_children():child.destroy()
            if not a:continue
            if title=='Cartridge' and a.kind!='cartridge':continue
            if title!='Cartridge' and a.kind!='weapon':continue
            keys=CARTRIDGE_FIELDS if title=='Cartridge' else dict(WEAPON_SECTIONS)[title]
            for i,key in enumerate(keys):
                ttk.Label(form.inner,text=key.replace('_',' ').capitalize()).grid(row=i,column=0,sticky='w',padx=(0,12),pady=4)
                v=tk.StringVar(value=a.fields.get(key,''))
                if key in ENUMS:
                    w=ttk.Combobox(form.inner,textvariable=v,values=ENUMS[key],width=43)
                    w.bind('<<ComboboxSelected>>',lambda _,k=key,var=v:self._set(k,var.get()))
                else:
                    w=ttk.Entry(form.inner,textvariable=v,width=46)
                w.grid(row=i,column=1,sticky='ew',pady=4)
                w.bind('<FocusOut>',lambda _,k=key,var=v:self._set(k,var.get()))
                w.bind('<Return>',lambda _,k=key,var=v:self._set(k,var.get()))
                if key=='id':w.configure(state='disabled')
            form.inner.grid_columnconfigure(1,weight=1)
        for tab in self.tabs.tabs():
            name=self.tabs.tab(tab,'text')
            self.tabs.tab(tab,state='normal' if (not a or (name=='Cartridge')==(a.kind=='cartridge') or name in ('Compatibility','Ballistic Test','Model / Animations')) else 'disabled')
        self._show_compat();self._draw_preview();self._show_model()
    def _show_compat(self):
        for child in self.compat.winfo_children():child.destroy()
        a=self._asset()
        if not a or a.kind!='weapon':
            ttk.Label(self.compat,text='Select a weapon to set compatible cartridges and muzzle velocities.').pack(anchor='w')
            return
        ttk.Label(self.compat,text='Select which cartridges this weapon accepts. A cartridge may be shared between weapons.').pack(anchor='w',pady=(0,12))
        columns=('cartridge','chamber','enabled','velocity'); table=ttk.Treeview(self.compat,columns=columns,show='headings',height=12)
        for col,width in zip(columns,(210,170,110,170)):
            table.heading(col,text=col.title());table.column(col,width=width)
        table.pack(fill='both',expand=True)
        for cid,c in self.project.cartridges.items():
            table.insert('','end',iid=cid,values=(cid,c.fields.get('chamber',''),
                'YES' if cid in a.accepted else 'NO',a.accepted.get(cid,'—')))
        actions=ttk.Frame(self.compat);actions.pack(fill='x',pady=8)
        def enable():
            selected=table.selection()
            if not selected:return
            cid=selected[0]
            velocity=simpledialog.askstring('Muzzle velocity',f'{cid} fired from {a.id} (m/s)',
                initialvalue=a.accepted.get(cid,self.project.cartridges[cid].fields.get('reference_velocity_mps','360')),
                parent=self)
            if velocity is None:return
            try: positive_float(velocity,.01,10000,'velocity')
            except ValueError as e: messagebox.showerror('Invalid velocity',str(e));return
            self._record();a.accepted[cid]=velocity;self._show_compat();self._draw_preview()
        def disable():
            selected=table.selection()
            if not selected:return
            cid=selected[0]
            if cid==a.fields.get('default_cartridge'):
                messagebox.showerror('Cannot remove default','Change the default cartridge first.');return
            if cid in a.accepted:
                self._record();del a.accepted[cid];self._show_compat();self._draw_preview()
        def make_default():
            selected=table.selection()
            if not selected:return
            cid=selected[0]
            if cid not in a.accepted:messagebox.showerror('Not accepted','Enable this cartridge first.');return
            self._set('default_cartridge',cid);self._set('ammo_name',cid);self._show_compat()
        ttk.Button(actions,text='Allow / Edit velocity…',command=enable).pack(side='left',padx=3)
        ttk.Button(actions,text='Disallow',command=disable).pack(side='left',padx=3)
        ttk.Button(actions,text='Set default',command=make_default).pack(side='left',padx=3)
        ttk.Label(self.compat,text='The exported accept = cartridge_id:velocity rows are authoritative.').pack(anchor='w')
    def _draw_preview(self):
        for child in self.preview.winfo_children():child.destroy()
        a=self._asset()
        if not a:return
        if a.kind=='weapon':
            cid=a.fields.get('default_cartridge');cartridge=self.project.cartridges.get(cid)
            velocity=a.accepted.get(cid,'')
        else: cartridge=a;velocity=a.fields.get('reference_velocity_mps','')
        if not cartridge:return
        ttk.Label(self.preview,text=f"{cartridge.id}  ·  {velocity} m/s (illustrative free-flight trajectory; no collision)").pack(anchor='w')
        canvas=tk.Canvas(self.preview,bg='#161e2b',height=355,highlightthickness=0)
        canvas.pack(fill='both',expand=True,pady=8)
        output=ttk.Label(self.preview);output.pack(anchor='w')
        def draw(_=None):
            canvas.delete('all')
            try: samples=flight(cartridge,float(velocity),max_seconds=2.0)
            except (ValueError,ZeroDivisionError,KeyError) as exc:
                output.configure(text='Unable to preview: '+str(exc));return
            W=max(canvas.winfo_width(),400);H=max(canvas.winfo_height(),240)
            maxx=max(p[0] for p in samples); miny=min(p[1] for p in samples)
            xscale=(W-70)/max(maxx,1);yscale=(H-70)/max(abs(miny),1)
            origin_y=36
            canvas.create_line(38,origin_y,W-20,origin_y,fill='#8899ad',dash=(4,5))
            coords=[]
            for x,y in samples:coords.extend([38+x*xscale,origin_y-y*yscale])
            if len(coords)>=4:canvas.create_line(*coords,fill='#ffbf69',width=3,smooth=True)
            canvas.create_text(45,origin_y-12,text='Muzzle',anchor='w',fill='white')
            canvas.create_text(W-12,H-16,text=f'{maxx:.0f} m distance',anchor='e',fill='white')
            output.configure(text=f'Flight time 2.0 s · Drop {abs(miny):.2f} m · Preview uses 9.80665 m/s² gravity and 1.225 kg/m³ air density')
        canvas.bind('<Configure>',draw);self.after_idle(draw)
    def _show_model(self):
        for child in self.model.winfo_children():child.destroy()
        a=self._asset()
        if not a or a.kind!='weapon':return
        ttk.Label(self.model,text='Standalone GoldSrc MDL metadata and interactive bounds preview (not a rendered/skinned mesh).').pack(anchor='w')
        modelname=tk.StringVar(value=a.fields.get('view_model',''))
        ttk.Combobox(self.model,textvariable=modelname,
              values=[a.fields.get(k,'') for k in ('view_model','player_model','world_model')],width=68).pack(anchor='w',pady=8)
        info=ttk.Label(self.model,text='Select a local model to preview its bounds and animation sequence names.');info.pack(anchor='w')
        viewport=tk.Canvas(self.model,bg='#141b2b',height=280);viewport.pack(fill='both',expand=True)
        seq=tk.Listbox(self.model,height=6);seq.pack(fill='x',pady=6)
        angle=[0]
        def open_model():
            path=self.project.root/modelname.get()
            try: mdl=ModelInfo(path)
            except (OSError,ValueError) as e:messagebox.showerror('Model preview',str(e));return
            info.configure(text=f'{mdl.name}  ·  sequence count: {mdl.sequence_count}  ·  bounds {mdl.mins} → {mdl.maxs}')
            seq.delete(0,'end')
            for label,fps,frames in mdl.sequences:seq.insert('end',f'{label}  ({frames} frames at {fps:g} fps)')
            bounds=[(mdl.mins[0],mdl.mins[1],mdl.mins[2]),(mdl.maxs[0],mdl.maxs[1],mdl.maxs[2])]
            def draw():
                viewport.delete('all');W=max(viewport.winfo_width(),400);H=max(viewport.winfo_height(),240)
                lo,hi=bounds; scale=140/max(1,*[hi[i]-lo[i] for i in range(3)])
                points=[]
                for ix in (0,1):
                    for iy in (0,1):
                        for iz in (0,1):
                            x=(lo[0] if ix==0 else hi[0])-(lo[0]+hi[0])*.5
                            y=(lo[1] if iy==0 else hi[1])-(lo[1]+hi[1])*.5
                            z=(lo[2] if iz==0 else hi[2])-(lo[2]+hi[2])*.5
                            t=math.radians(angle[0]);xr=x*math.cos(t)-y*math.sin(t)
                            yr=x*math.sin(t)+y*math.cos(t)
                            points.append((W*.5+(xr-yr*.4)*scale,H*.5-(z+yr*.35)*scale))
                for i,p in enumerate(points):
                    for axis in (1,2,4):
                        j=i^axis
                        if i<j:viewport.create_line(*p,*points[j],fill='#6dd3c8',width=2)
                viewport.create_text(12,12,text='Bounding box · drag to rotate',anchor='nw',fill='#eef4fc')
            def spin(e):angle[0]=(angle[0]+e.x*.05)%360;draw()
            viewport.bind('<B1-Motion>',spin);viewport.bind('<Configure>',lambda e:draw());draw()
        ttk.Button(self.model,text='Load model metadata / bounds',command=open_model).pack(anchor='w',pady=4)
    def check(self):
        errs=validate(self.project.weapons,self.project.cartridges)
        if errs:messagebox.showerror('Validation failed','\n'.join(errs[:24]));return
        if self.validator.exists():
            import tempfile,subprocess
            with tempfile.TemporaryDirectory() as p:
                try:ProjectSnapshot(self.project).save(Path(p))
                except Exception as e:messagebox.showerror('Validation failed',str(e));return
                result=subprocess.run([str(self.validator),p],capture_output=True,text=True)
                if result.returncode:messagebox.showerror('Native registry validation',result.stdout+result.stderr);return
        messagebox.showinfo('Validation','Basic schema and compatibility checks passed.\nSave / Export runs native registry validation if built.')
    def save(self):
        try:
            if not self.validator.exists():
                messagebox.showerror('Validator required',
                    'Build the standalone C++ validator first:\ncmake -S tools/weapon_editor -B tools/weapon_editor/build\ncmake --build tools/weapon_editor/build\nExport is blocked to avoid producing files rejected by the game.')
                return
            self.project.save(self.validator)
            self.undo.clear();self.redo.clear();self.dirty=False
            self.status.set(f'Saved: {len(self.project.weapons)} weapons / {len(self.project.cartridges)} cartridges to {self.project.root}')
        except Exception as e:messagebox.showerror('Export failed',str(e))

class ProjectSnapshot:
    def __init__(self,source):self.source=source
    def save(self,root):
        tmp=Project(root)
        tmp.weapons=copy.deepcopy(self.source.weapons)
        tmp.cartridges=copy.deepcopy(self.source.cartridges)
        tmp.save(None)

def main(root=None):
    app=Editor(root)
    app.mainloop()
