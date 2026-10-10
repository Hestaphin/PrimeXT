import os
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from project import Project, Asset, parse, validate, flight

VALIDATOR=Path(os.environ.get('PRIMEXT_WEAPON_VALIDATOR',str(ROOT/'build'/'primext-weapon-validate')))
SOURCE=ROOT.parents[1]/'documentation'/'examples'/'modular-ballistics'

class ProjectTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.project=Project(Path(self.tmp.name))
        self.c=self.project.add('cartridge','test_fmj')
        self.w=self.project.add('weapon','test_gun')
        self.w.accepted['test_fmj']='385'
        self.w.fields['default_cartridge']='test_fmj'
        self.w.fields['ammo_name']='test_fmj'
        self.validator=VALIDATOR.resolve()

    def test_create_save_and_reload(self):
        self.assertEqual(validate(self.project.weapons,self.project.cartridges),[])
        self.project.save(self.validator)
        loaded=Project(self.project.root)
        loaded.load()
        self.assertEqual(loaded.weapons['test_gun'].accepted['test_fmj'],'385')
        self.assertEqual(loaded.cartridges['test_fmj'].fields['mass_grams'],'8.04')
        self.assertEqual((self.project.root/'scripts/weapons/manifest.txt').read_text(),'test_gun\n')

    def test_shared_ammo_multiple_weapons_and_velocity_override(self):
        self.w.accepted['test_fmj']='385'
        other=self.project.add('weapon','test_smg')
        other.accepted['test_fmj']='420'
        other.fields['default_cartridge']='test_fmj';other.fields['ammo_name']='test_fmj'
        self.project.save(self.validator)
        r=Project(self.project.root);r.load()
        self.assertEqual(r.cartridges.keys(),{'test_fmj'})
        self.assertEqual(r.weapons['test_gun'].accepted['test_fmj'],'385')
        self.assertEqual(r.weapons['test_smg'].accepted['test_fmj'],'420')

    def test_no_write_on_native_rejection(self):
        self.project.save(self.validator)
        old=(self.project.root/'scripts/weapons/test_gun.weapon').read_text()
        self.w.fields['primary_mode']='invalid'
        with self.assertRaisesRegex(ValueError,'Native registry rejected'):
            self.project.save(self.validator)
        self.assertEqual((self.project.root/'scripts/weapons/test_gun.weapon').read_text(),old)

    def test_invalid_compatibility_rejected(self):
        self.w.accepted['missing_cartridge']='350'
        with self.assertRaises(ValueError):self.project.save(self.validator)

    def test_reserved_id_and_path_traversal(self):
        self.w.fields['runtime_id']='63'
        with self.assertRaises(ValueError):self.project.save(self.validator)
        self.w.fields['runtime_id']='32'
        self.w.fields['view_model']='../../malicious.mdl'
        with self.assertRaises(ValueError):self.project.save(self.validator)

    def test_case_insensitive_cartridge_alias_rejected(self):
        self.project.add('cartridge', 'TEST_FMJ')
        with self.assertRaisesRegex(ValueError, 'case-insensitive'):
            self.project.save(self.validator)

    def test_duplicate_runtime_id_rejected(self):
        x=self.project.add('weapon','another')
        x.fields['runtime_id']='32'
        with self.assertRaises(ValueError):self.project.save(self.validator)

    def test_deletion_rejects_referenced_cartridge(self):
        with self.assertRaises(ValueError):self.project.remove('cartridge','test_fmj')

    def test_roundtrip_authoritative_sample(self):
        original=Project(SOURCE);original.load()
        self.project.weapons=original.weapons;self.project.cartridges=original.cartridges
        self.project.save(self.validator)
        other=Project(self.project.root);other.load()
        for id in original.weapons:
            self.assertEqual(other.weapons[id].serialize(),original.weapons[id].serialize())

    def test_manifest_safety(self):
        self.project.save(self.validator)
        manifest=self.project.root/'scripts/weapons/manifest.txt'
        manifest.write_text('../outside\n')
        with self.assertRaises(ValueError):Project(self.project.root).load()

    def test_constant_gravity_without_drag(self):
        self.c.fields['drag_coefficient']='0'
        a=flight(self.c,300)
        self.c.fields['mass_grams']='16.08'
        b=flight(self.c,300)
        self.assertEqual(a,b)

    def test_air_drag_reduces_distance(self):
        p=flight(self.c,300)
        self.c.fields['drag_coefficient']='0'
        q=flight(self.c,300)
        self.assertGreater(q[-1][0],p[-1][0])

    def test_duplicate_file_field_error(self):
        with self.assertRaisesRegex(ValueError,'duplicate'):
            parse('id = foo\nid = bar\n','weapon')

if __name__=='__main__':unittest.main()
