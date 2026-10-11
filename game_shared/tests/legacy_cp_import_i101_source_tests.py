#!/usr/bin/env python3
"""Source integration check; not a substitute for running pxmv or ASan."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
editor = (root / 'utils/pxmv/PhysicsAssetEditor.cpp').read_text()
validator = (root / 'utils/pxmv/LegacyCpStudioValidator.h').read_text()
assert '#include "LegacyCpStudioValidator.h"' in editor
assert 'if(!legacycp::Validate(bytes.data(), bytes.size(), error)) return false;' in editor
assert editor.index('legacycp::Validate(bytes.data()') < editor.index('BuildLegacyReferenceBones(hdr,')
assert 'kMaxProxyBytes' in editor
for label in ('bone table', 'sequence table', 'sequence-group table', 'animation table',
              'bodypart table', 'model table', 'vertex/mesh table', 'triangle command'):
    assert label in validator, label
assert 'triangleBudget' in validator
assert 'View view(fileBytes, static_cast<size_t>(header->length))' in validator
print('I.10.1 _cp importer validation source wiring PASS')
