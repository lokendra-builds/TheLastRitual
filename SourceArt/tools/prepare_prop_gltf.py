"""Builds single-mesh glTF copies of the downloaded Poly Haven props under SourceArt/prepared/props/<name>/.

Interchange imports every glTF node as its own static mesh (lid, latch, handle...). This merges all
nodes into one mesh: each node's translation is baked into a copy of its POSITION data (the props only
use translations; anything else is refused). Pure JSON + struct; run with:
    python3 -I SourceArt/tools/prepare_prop_gltf.py <name> [<name> ...]
"""
import json
import os
import shutil
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
RES = '1k'


def prepare(name):
    src = os.path.join(ROOT, 'downloads', name)
    out = os.path.join(ROOT, 'prepared', 'props', name)
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(os.path.join(out, 'textures'))
    g = json.load(open(os.path.join(src, '%s_%s.gltf' % (name, RES))))
    if len(g['buffers']) != 1:
        raise RuntimeError(name + ': expected one buffer')
    data = bytearray(open(os.path.join(src, os.path.normpath(g['buffers'][0]['uri'])), 'rb').read())

    prims = []
    for idx in g['scenes'][0]['nodes']:
        n = g['nodes'][idx]
        if n.get('children') or 'matrix' in n or 'rotation' in n or 'scale' in n:
            raise RuntimeError('%s: node %s has more than a translation' % (name, n['name']))
        t = n.get('translation', [0.0, 0.0, 0.0])
        for p in g['meshes'][n['mesh']]['primitives']:
            p = json.loads(json.dumps(p))
            if any(abs(v) > 1e-7 for v in t):
                acc = g['accessors'][p['attributes']['POSITION']]
                bv = g['bufferViews'][acc['bufferView']]
                stride = bv.get('byteStride', 12)
                base = bv.get('byteOffset', 0) + acc.get('byteOffset', 0)
                moved = bytearray()
                for i in range(acc['count']):
                    x, y, z = struct.unpack_from('<3f', data, base + i * stride)
                    moved += struct.pack('<3f', x + t[0], y + t[1], z + t[2])
                while len(data) % 4:
                    data.append(0)
                g['bufferViews'].append({'buffer': 0, 'byteOffset': len(data), 'byteLength': len(moved)})
                data += moved
                new_acc = dict(acc)
                new_acc.pop('byteOffset', None)
                new_acc['bufferView'] = len(g['bufferViews']) - 1
                new_acc['min'] = [acc['min'][k] + t[k] for k in range(3)]
                new_acc['max'] = [acc['max'][k] + t[k] for k in range(3)]
                g['accessors'].append(new_acc)
                p['attributes']['POSITION'] = len(g['accessors']) - 1
            prims.append(p)
    g['meshes'] = [{'name': name, 'primitives': prims}]
    g['nodes'] = [{'name': name, 'mesh': 0}]
    g['scenes'] = [{'name': name, 'nodes': [0]}]
    g['scene'] = 0
    g['buffers'][0]['uri'] = name + '.bin'
    g['buffers'][0]['byteLength'] = len(data)
    open(os.path.join(out, name + '.bin'), 'wb').write(data)
    for im in g.get('images', []):
        rel = os.path.normpath(im['uri'])
        if rel.startswith('..') or os.path.isabs(rel):
            raise RuntimeError('bad image path ' + rel)
        shutil.copy(os.path.join(src, rel), os.path.join(out, 'textures', os.path.basename(rel)))
        im['uri'] = 'textures/' + os.path.basename(rel)
    json.dump(g, open(os.path.join(out, name + '.gltf'), 'w'), indent=1)
    print('prepared', name, len(prims), 'primitives')


for a in sys.argv[1:]:
    prepare(a)
