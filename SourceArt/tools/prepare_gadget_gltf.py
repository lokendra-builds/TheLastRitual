"""Builds trimmed glTF copies of downloaded Poly Haven models under SourceArt/prepared/<Name>/.
Pure JSON manipulation; run with: python3 -I SourceArt/tools/prepare_gadget_gltf.py"""
import json, os, shutil, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

def prepare(src_dir, src_name, out_name, keep_nodes=None, y_shift=0.0):
    src = os.path.join(ROOT, 'downloads', src_dir)
    out = os.path.join(ROOT, 'prepared', out_name)
    os.makedirs(os.path.join(out, 'textures'), exist_ok=True)
    g = json.load(open(os.path.join(src, src_name + '.gltf')))
    if keep_nodes is not None:
        idx = [i for i, n in enumerate(g['nodes']) if n['name'] in keep_nodes]
        for i in idx:
            n = g['nodes'][i]
            t = n.get('translation', [0, 0, 0])
            n['translation'] = [t[0], t[1] - y_shift, t[2]]
        nodes = [g['nodes'][i] for i in idx]
        meshes = []
        for n in nodes:
            meshes.append(g['meshes'][n['mesh']])
            n['mesh'] = len(meshes) - 1
        g['nodes'], g['meshes'] = nodes, meshes
        g['scenes'][0]['nodes'] = list(range(len(nodes)))
    for b in g['buffers']:
        shutil.copy(os.path.join(src, b['uri']), os.path.join(out, os.path.basename(b['uri'])))
        b['uri'] = os.path.basename(b['uri'])
    for im in g.get('images', []):
        shutil.copy(os.path.join(src, im['uri']), os.path.join(out, 'textures', os.path.basename(im['uri'])))
        im['uri'] = 'textures/' + os.path.basename(im['uri'])
    json.dump(g, open(os.path.join(out, out_name + '.gltf'), 'w'), indent=1)
    print('wrote', out_name, g['scenes'][0]['nodes'])

prepare('small_plastic_torch', 'small_plastic_torch', 'SM_Torch')
prepare('service_pistol', 'service_pistol', 'SM_Pistol',
        keep_nodes={'service_pistol_pistol_a', 'service_pistol_slide_a', 'service_pistol_hammer_a', 'service_pistol_trigger_a'},
        y_shift=0.0)

# UV torch: same torch, plastic body re-coloured from yellow to deep violet (black rubber/chrome untouched).
prepare('small_plastic_torch', 'small_plastic_torch', 'SM_UVTorch')
from PIL import Image
import colorsys
_diff = os.path.join(ROOT, 'prepared', 'SM_UVTorch', 'textures', 'small_plastic_torch_diff_2k.jpg')
_im = Image.open(_diff).convert('RGB')
_hsv = _im.convert('HSV')
_h, _s, _v = _hsv.split()
# Yellow plastic has strong saturation; push its hue to violet (~0.76 -> 194/255) and darken a little.
_mask = _s.point(lambda x: 255 if x > 70 else 0)
_h2 = _h.point(lambda x: 194)
_v2 = _v.point(lambda x: int(x * 0.62))
_s2 = _s.point(lambda x: int(min(255, x * 0.85)))
_violet = Image.merge('HSV', (_h2, _s2, _v2)).convert('RGB')
Image.composite(_violet, _im, _mask).save(_diff, quality=93)
print('recoloured', _diff)

# Gun weapon light: the same torch with its plastic turned to matt black anodised metal.
prepare('small_plastic_torch', 'small_plastic_torch', 'SM_WeaponLight')
_diff = os.path.join(ROOT, 'prepared', 'SM_WeaponLight', 'textures', 'small_plastic_torch_diff_2k.jpg')
_im = Image.open(_diff).convert('RGB')
_h, _s, _v = _im.convert('HSV').split()
_mask = _s.point(lambda x: 255 if x > 70 else 0)
_black = Image.merge('HSV', (_h, _s.point(lambda x: 0), _v.point(lambda x: int(18 + x * 0.10)))).convert('RGB')
Image.composite(_black, _im, _mask).save(_diff, quality=93)
print('blackened', _diff)
