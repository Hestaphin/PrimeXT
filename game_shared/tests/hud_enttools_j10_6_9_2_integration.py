#!/usr/bin/env python3
"""Exercise production HUD, entity creation, callback dispatch and placement.

Engine services/factories are explicit adapters. Placement runs the actual
PrimeXT transform, keyvalue and weapon materialization methods and math code.
These tests do not replace Windows linking or live-map pickup acceptance.
"""
from pathlib import Path
import argparse
import json
import os
import re
import subprocess
import tempfile

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parents[1]


def function(source, signature):
    """Extract a whole definition, ignoring braces in comments and strings."""
    start = source.index(signature)
    brace = source.index('{', start)
    while ';' in source[start:brace]:
        start = source.index(signature, start + len(signature))
        brace = source.index('{', start)
    cursor, depth, state = brace, 0, 'code'
    while cursor < len(source):
        char = source[cursor]
        pair = source[cursor:cursor + 2]
        if state == 'line':
            if char == '\n': state = 'code'
        elif state == 'block':
            if pair == '*/': state = 'code'; cursor += 1
        elif state in ('"', "'"):
            if char == '\\': cursor += 1
            elif char == state: state = 'code'
        elif pair == '//': state = 'line'; cursor += 1
        elif pair == '/*': state = 'block'; cursor += 1
        elif char in ('"', "'"): state = char
        elif char == '{': depth += 1
        elif char == '}':
            depth -= 1
            if not depth: return source[start:cursor + 1]
        cursor += 1
    raise ValueError('Unclosed definition: ' + signature)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--primext-tree', type=Path, default=ROOT)
    parser.add_argument('--xash-tree', type=Path)
    parser.add_argument('--ubsan', action='store_true')
    parser.add_argument('--record', type=Path)
    args = parser.parse_args()
    prime = args.primext_tree.resolve()
    xash = (args.xash_tree or prime.parent / 'xash3d-fwgs-master').resolve()
    fixtures = TESTS / 'fixtures/j10_6_9_2'
    ammo = (prime / 'client/ammo.cpp').read_text()
    client = (xash / 'engine/server/sv_client.c').read_text()
    game = (xash / 'engine/server/sv_game.c').read_text()
    commands = (xash / 'engine/common/cmd.c').read_text()
    registration = (xash / 'engine/client/cl_main.c').read_text()
    base = (prime / 'server/cbase.cpp').read_text()
    weapons = (prime / 'server/weapons.cpp').read_text()
    save = (prime / 'server/saverestore.cpp').read_text()

    hud_signatures = ['int WeaponsResource :: HasAmmo',
        'WEAPON *WeaponsResource :: GetFirstPos',
        'WEAPON* WeaponsResource :: GetNextActivePos',
        'void CHudAmmo::Think', 'void WeaponsResource :: SelectSlot']
    if 'static void SelectHudWeapon' in ammo:
        hud_signatures.insert(0, 'static void SelectHudWeapon')
    hud = '\n\n'.join(function(ammo, item) for item in hud_signatures)

    helpers = '\n\n'.join(function(client, item) for item in [
        'static qboolean SV_EntCreateIsAlive', 'static qboolean SV_EntToolSetOrigin']
        if item in client)
    allocation = [function(game, 'static edict_t* SV_AllocPrivateData'),
        function(game, 'edict_t* SV_CreateNamedEntity(')]
    if 'edict_t* SV_CreateNamedEntityQuiet' in game:
        allocation.append(function(game, 'edict_t* SV_CreateNamedEntityQuiet'))
    engine = '\n\n'.join(allocation + [
        function(game, 'int GAME_EXPORT pfnDropToFloor'),
        function(client, 'static void SV_EntSendVars'), helpers,
        function(client, 'static qboolean SV_EntCreate_f')])
    callback = '\n\n'.join(function(commands, item) for item in [
        'static void Cbuf_InsertTextToBuffer', 'void Cmd_Null_f',
        'static qboolean Cmd_ShouldAllowCommand',
        'static void Cmd_ExecuteStringWithPrivilegeCheck'])
    registration_line = next((line.strip() for line in registration.splitlines()
        if 'Cmd_AddCommand(' in line and '"ent_getvars_cb"' in line), '')
    placement = '\n\n'.join(function(base, item) for item in [
        'const Vector& CBaseEntity :: GetAbsOrigin',
        'void CBaseEntity::SetAbsOrigin',
        'void CBaseEntity::CalcAbsolutePosition',
        'void CBaseEntity::SetLocalAngles'])
    placement += '\n\n' + function(save, 'void EntvarsKeyvalue')
    placement += '\n\n' + '\n\n'.join(function(weapons, item) for item in [
        'void CBasePlayerItem::FallThink', 'void CBasePlayerItem::Materialize'])

    records = []
    with tempfile.TemporaryDirectory(prefix='j10692_methods_') as temporary:
        temporary = Path(temporary)
        # The supplied Xash ZIP lacks the build.h submodule header. For these
        # method tests use the provided PrimeXT platform header explicitly.
        (temporary / 'build.h').write_bytes((prime / 'public/build.h').read_bytes())
        specs = [
            ('hud', 'cpp', hud, {'@REGISTRATION@': ''}, [
                'normal-modular', 'normal-classic', 'fast-modular', 'fast-classic',
                'slot-cycle', 'menu-sentinel', 'no-suit', 'empty-name',
                'command-in-name', 'long-name']),
            ('enttools', 'c', engine, {}, [
                'restore', 'legacy', 'extension', 'unknown', 'normal-error',
                'spawn-reject', 'spawn-kill', 'spawn-free', 'spawn-recycle',
                'think-free', 'pre-key-free', 'post-key-free', 'named',
                'post-origin', 'no-think', 'no-keyvalue']),
            ('callback', 'c', callback, {'@REGISTRATION@': registration_line}, [
                'default', 'alias', 'filtered-alias', 'filtered-privileged']),
            ('placement', 'cpp', placement, {
                '@ENGINE_HELPERS@': helpers,
                '@SYNC_ORIGIN@': 'assert(SV_EntToolSetOrigin(&entity, requested));'
                    if helpers else 'entity.v.origin = requested;',
                '@SYNC_FLOOR@': 'assert(SV_EntToolSetOrigin(&entity, entity.v.origin));'
                    if helpers else '',
            }, ['spawn-origin', 'floor-trigger', 'stale-keyvalue']),
        ]
        for name, extension, production, substitutions, cases in specs:
            template = (fixtures / (name + '_adapter.' + extension + '.in')).read_text()
            text = template.replace('@PRODUCTION@', production)
            for key, value in substitutions.items(): text = text.replace(key, value)
            source = temporary / (name + '.' + extension)
            binary = temporary / name
            source.write_text(text)
            flags = ['-O0', '-g', '-Wall', '-Wextra', '-Wno-unused-function',
                '-Wno-unused-parameter', '-Wno-sign-compare', '-Wno-missing-field-initializers']
            if args.ubsan: flags += ['-fsanitize=undefined', '-fno-sanitize-recover=all']
            extra = []
            if name in ('enttools', 'callback'):
                compiler = os.environ.get('CC', 'cc')
                flags += ['-std=c99', '-I' + str(temporary), *[
                    '-I' + str(xash / part) for part in ['common', 'engine', 'public']]]
                extra += ['-lm']
            else:
                compiler = os.environ.get('CXX', 'c++')
                flags += ['-std=c++17', '-Wno-write-strings', '-Wno-invalid offsetof'.replace(' ', '-'),
                    '-Wno-deprecated-copy', '-Wno-unknown-pragmas',
                    '-D_LINUX', '-DLINUX', '-Dstricmp=strcasecmp', '-Dstrnicmp=strncasecmp',
                    '-D_snprintf=snprintf', '-D_vsnprintf=vsnprintf',
                    *['-I' + str(prime / part) for part in [
                        'client', 'server', 'game_shared', 'common', 'engine', 'public']]]
                if name == 'placement':
                    flags += ['-ffunction-sections', '-fdata-sections']
                    extra += [str(prime / 'game_shared' / filename) for filename in [
                        'vector.cpp', 'matrix.cpp', 'mathlib.cpp', 'stringlib.cpp']]
                    extra += ['-Wl,--gc-sections']
            command = [compiler, *flags, str(source), *extra, '-o', str(binary)]
            build = subprocess.run(command, capture_output=True, text=True)
            if build.returncode:
                output = build.stdout + build.stderr
                errors = '\n'.join(line for line in output.splitlines() if 'error:' in line)
                raise RuntimeError(name + ' compile failed:\n' + errors + '\n' + output[-6000:])
            for case in cases:
                result = subprocess.run([str(binary), case], capture_output=True, text=True)
                output = (result.stdout + result.stderr).strip()
                records.append({'group': name, 'case': case, 'returncode': result.returncode,
                    'output': output[-2000:]})
                print(('PASS ' if not result.returncode else 'FAIL ') + name + '/' + case +
                    (': ' + output[-1000:] if output else ''), flush=True)
    if args.record:
        args.record.parent.mkdir(parents=True, exist_ok=True)
        args.record.write_text(json.dumps(records, indent=2) + '\n')
    passed = sum(item['returncode'] == 0 for item in records)
    print(f'J.10.6.9.2 method cases: {passed}/{len(records)} passed', flush=True)
    return 0 if passed == len(records) else 1


if __name__ == '__main__':
    raise SystemExit(main())
