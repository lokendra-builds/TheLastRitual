"""Downloads the CC0 Poly Haven textures and models used by the haveli (R21).

Run OUTSIDE the editor, isolated:   python3 -I Scripts/download_polyhaven.py
Each asset goes into its own folder SourceArt/downloads/<asset>/ (untrusted data: never executed).
SourceArt/polyhaven_manifest.json records name, authors, dimensions and page URL for the licence list.
Already-downloaded files are skipped.
"""
import json
import os
import urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
DL = os.path.join(ROOT, 'SourceArt', 'downloads')
MANIFEST = os.path.join(ROOT, 'SourceArt', 'polyhaven_manifest.json')
UA = {'User-Agent': 'asset-fetch/1.0'}

# Tileable PBR sets: diffuse, DirectX normal (UE convention), ARM (AO / roughness / metal).
TEXTURES = ['rough_plaster_broken', 'worn_tile_floor', 'red_sandstone_pavement', 'rough_wood',
            'sandstone_cracks', 'rustic_stone_wall']
TEX_RES = '2k'
# Models (glTF, 1k textures).
MODELS = ['planter_pot_clay', 'ceramic_pot', 'ceramic_vase_04', 'wooden_crate_01', 'wooden_crate_02',
          'Lantern_01', 'wooden_lantern_01', 'brass_diya_lantern', 'treasure_chest', 'spinning_wheel_01',
          'vintage_grandfather_clock_01', 'wooden_bucket_01', 'brass_pot_01', 'brass_vase_03',
          'WoodenTable_02', 'wooden_stool_02', 'folding_wooden_stool', 'wicker_basket_01',
          'vintage_wooden_drawer_01']
MODEL_RES = '1k'


def get_json(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
        return json.load(r)


def fetch(url, dest):
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        return
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=300) as r, open(dest + '.part', 'wb') as f:
        f.write(r.read())
    os.replace(dest + '.part', dest)
    print('  got', os.path.relpath(dest, ROOT))


def main():
    manifest = json.load(open(MANIFEST)) if os.path.exists(MANIFEST) else {}
    for kind, names in (('texture', TEXTURES), ('model', MODELS)):
        for name in names:
            print(kind, name)
            info = get_json('https://api.polyhaven.com/info/' + name)
            files = get_json('https://api.polyhaven.com/files/' + name)
            out = os.path.join(DL, name)
            if kind == 'texture':
                for key, short in (('Diffuse', 'diff'), ('nor_dx', 'nor_dx'), ('arm', 'arm')):
                    fetch(files[key][TEX_RES]['jpg']['url'], os.path.join(out, '%s_%s_%s.jpg' % (name, short, TEX_RES)))
            else:
                g = files['gltf'][MODEL_RES]['gltf']
                fetch(g['url'], os.path.join(out, os.path.basename(g['url'])))
                for rel, inc in g.get('include', {}).items():
                    rel = os.path.normpath(rel)
                    if rel.startswith('..') or os.path.isabs(rel):
                        raise RuntimeError('bad include path ' + rel)
                    fetch(inc['url'], os.path.join(out, rel))
            manifest[name] = {'kind': kind, 'title': info.get('name', name), 'authors': list(info.get('authors', {}).keys()),
                              'dimensions_mm': info.get('dimensions'), 'url': 'https://polyhaven.com/a/' + name,
                              'licence': 'CC0'}
    json.dump(manifest, open(MANIFEST, 'w'), indent=1)
    print('manifest ->', MANIFEST)


if __name__ == '__main__':
    main()
