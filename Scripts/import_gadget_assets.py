"""Imports the first-person gadget models and HUD icons.

Run inside the editor:
  UnrealEditor TheLastRitual.uproject -ExecutePythonScript="$PWD/Scripts/import_gadget_assets.py" -unattended -nosplash

Sources (all CC0, see context/asset-licences.md):
  SourceArt/prepared/SM_Torch/SM_Torch.gltf   <- Poly Haven "Small Plastic Torch"
  SourceArt/prepared/SM_Pistol/SM_Pistol.gltf <- Poly Haven "Service Pistol" (wood-grip variant only;
                                                 imports as 4 part meshes sharing one origin: frame, slide, hammer, trigger)
  SourceArt/prepared/SM_UVTorch/SM_UVTorch.gltf <- the same torch with its plastic re-coloured violet
  SourceArt/prepared/SM_WeaponLight/SM_WeaponLight.gltf <- the same torch blackened, used as the Gun Torch's under-barrel light
  SourceArt/icons/T_Icon_*.png                <- SourceArt/tools/make_gadget_icons.py
The prepared glTFs are made by SourceArt/tools/prepare_gadget_gltf.py.
"""
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(PROJECT, 'SourceArt')

MODELS = [
    # (source file, destination folder, asset name)
    ('prepared/SM_Torch/SM_Torch.gltf', '/Game/Gadgets/Torch', 'SM_Torch'),
    ('prepared/SM_Pistol/SM_Pistol.gltf', '/Game/Gadgets/GunTorch', 'SM_Pistol'),
    ('prepared/SM_UVTorch/SM_UVTorch.gltf', '/Game/Gadgets/UVTorch', 'SM_UVTorch'),
    ('prepared/SM_WeaponLight/SM_WeaponLight.gltf', '/Game/Gadgets/WeaponLight', 'SM_WeaponLight'),
]
ICONS = ['Torch', 'GunTorch', 'UVTorch', 'Thermometer', 'EMFMeter']


def log(msg):
    unreal.log('[import_gadget_assets] ' + msg)


def mesh_pipeline():
    """Interchange pipeline override: one combined static mesh, no collision, keep materials."""
    try:
        p = unreal.InterchangeGenericAssetsPipeline()
        mp = p.get_editor_property('mesh_pipeline')
        mp.set_editor_property('combine_static_meshes_behavior', unreal.InterchangeCombineStaticMeshesBehavior.ALL)
        mp.set_editor_property('collision', False)
        mp.set_editor_property('import_skeletal_meshes', False)
        p.get_editor_property('animation_pipeline').set_editor_property('import_animations', False)
        return p
    except Exception as e:  # property names differ between engine versions
        log('pipeline override unavailable (%s); using project defaults' % e)
        return None


def run_tasks(tasks):
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for t in tasks:
        paths = list(t.get_editor_property('imported_object_paths') or [])
        log('%s -> %s' % (os.path.basename(t.filename), paths))


def import_models():
    # Start clean so a previous un-combined import doesn't leave stray part meshes behind.
    for _, dest, _ in MODELS:
        if unreal.EditorAssetLibrary.does_directory_exist(dest):
            unreal.EditorAssetLibrary.delete_directory(dest)
    tasks = []
    for src, dest, name in MODELS:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(ART, src)
        t.destination_path = dest
        t.destination_name = name
        t.automated = True
        t.replace_existing = True
        t.replace_existing_settings = True
        t.save = True
        opt = mesh_pipeline()
        if opt:
            t.options = opt
        tasks.append(t)
    run_tasks(tasks)


def import_icons():
    tasks = []
    for g in ICONS:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(ART, 'icons', 'T_Icon_%s.png' % g)
        t.destination_path = '/Game/UI/Icons'
        t.destination_name = 'T_Icon_%s' % g
        t.automated = True
        t.replace_existing = True
        t.save = True
        tasks.append(t)
    run_tasks(tasks)
    for g in ICONS:
        path = '/Game/UI/Icons/T_Icon_%s' % g
        tex = unreal.EditorAssetLibrary.load_asset(path)
        if not tex:
            unreal.log_error('[import_gadget_assets] missing ' + path)
            continue
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property('srgb', True)
        tex.set_editor_property('never_stream', True)
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        log('icon %s %dx%d' % (path, tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))


def report():
    """Prints what landed in /Game/Gadgets: mesh bounds (for placing them in the view model) and material params."""
    for p in unreal.EditorAssetLibrary.list_assets('/Game/Gadgets', recursive=True):
        a = unreal.EditorAssetLibrary.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            b = a.get_bounding_box()
            mats = [m.get_editor_property('material_interface') for m in a.get_editor_property('static_materials')]
            log('MESH %s min=%s max=%s mats=%s' % (p, b.min, b.max, [m.get_path_name() if m else None for m in mats]))
            # View-model meshes: no collision, no Nanite needed.
            a.set_editor_property('light_map_resolution', 16)
        elif isinstance(a, unreal.MaterialInstance):
            parent = a.get_editor_property('parent')
            vec = [v.parameter_info.name for v in a.get_editor_property('vector_parameter_values')]
            sca = [v.parameter_info.name for v in a.get_editor_property('scalar_parameter_values')]
            log('MI %s parent=%s vec=%s scalar=%s' % (p, parent.get_path_name() if parent else None, vec, sca))
        else:
            log('ASSET %s (%s)' % (p, type(a).__name__))


import_models()
import_icons()
report()
log('done')
