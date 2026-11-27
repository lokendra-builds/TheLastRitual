"""Imports the CC0 Poly Haven textures and props for the haveli (R21).

  1. python3 -I Scripts/download_polyhaven.py          (downloads into SourceArt/downloads/<asset>/)
     python3 -I SourceArt/tools/prepare_prop_gltf.py <models...>   (merges each model's parts into one mesh)
  2. UnrealEditor TheLastRitual.uproject -ExecutePythonScript="$PWD/Scripts/import_haveli_art.py" -unattended -nosplash
  3. Scripts/create_pbr_materials.py (build_haveli.py runs it) builds the world-aligned materials.

Textures -> /Game/Art/Textures/<name>/T_<name>_{D,N,ARM}
Props    -> /Game/Art/Props/<name>/  (one combined static mesh SM_<name> + the glTF's material instances/textures,
            simple box collision so torch / gun traces and the player hit them)
Re-running replaces everything under those folders.
"""
import os
import sys

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(PROJECT, 'Scripts'))
DL = os.path.join(PROJECT, 'SourceArt', 'downloads')
PREP = os.path.join(PROJECT, 'SourceArt', 'prepared', 'props')   # single-mesh copies (SourceArt/tools/prepare_prop_gltf.py)

TEXTURES = ['rough_plaster_broken', 'worn_tile_floor', 'red_sandstone_pavement', 'rough_wood',
            'sandstone_cracks', 'rustic_stone_wall']
TEX_RES = '2k'
MODELS = ['planter_pot_clay', 'ceramic_pot', 'ceramic_vase_04', 'wooden_crate_01', 'wooden_crate_02',
          'Lantern_01', 'wooden_lantern_01', 'brass_diya_lantern', 'treasure_chest', 'spinning_wheel_01',
          'vintage_grandfather_clock_01', 'wooden_bucket_01', 'brass_pot_01', 'brass_vase_03',
          'WoodenTable_02', 'wooden_stool_02', 'folding_wooden_stool', 'wicker_basket_01',
          'vintage_wooden_drawer_01']
MODEL_RES = '1k'
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log('[import_haveli_art] ' + msg)


def task(filename, dest, name=None, options=None):
    t = unreal.AssetImportTask()
    t.filename = filename
    t.destination_path = dest
    if name:
        t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.replace_existing_settings = True
    t.save = True
    if options:
        t.options = options
    return t


def import_textures():
    tasks, post = [], []
    for name in TEXTURES:
        dest = '/Game/Art/Textures/' + name
        for short, suffix in (('diff', 'D'), ('nor_dx', 'N'), ('arm', 'ARM')):
            f = os.path.join(DL, name, '%s_%s_%s.jpg' % (name, short, TEX_RES))
            if not os.path.exists(f):
                unreal.log_error('[import_haveli_art] missing ' + f)
                continue
            asset = 'T_%s_%s' % (name, suffix)
            tasks.append(task(f, dest, asset))
            post.append(('%s/%s' % (dest, asset), suffix))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for path, suffix in post:
        tex = eal.load_asset(path)
        if not tex:
            unreal.log_error('[import_haveli_art] texture failed ' + path)
            continue
        if suffix == 'N':
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property('srgb', False)
            tex.set_editor_property('flip_green_channel', False)    # DirectX normal = UE convention
        elif suffix == 'ARM':
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property('srgb', False)
        else:
            tex.set_editor_property('srgb', True)
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD if suffix == 'D'
                                else (unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP if suffix == 'N' else unreal.TextureGroup.TEXTUREGROUP_WORLD_SPECULAR))
        eal.save_asset(path, only_if_is_dirty=False)
        log('texture %s %dx%d' % (path, tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))


def mesh_pipeline():
    try:
        p = unreal.InterchangeGenericAssetsPipeline()
        mp = p.get_editor_property('mesh_pipeline')
        mp.set_editor_property('combine_static_meshes_behavior', unreal.InterchangeCombineStaticMeshesBehavior.ALL)
        mp.set_editor_property('collision', False)
        mp.set_editor_property('import_skeletal_meshes', False)
        p.get_editor_property('animation_pipeline').set_editor_property('import_animations', False)
        return p
    except Exception as e:
        log('pipeline override unavailable (%s); using project defaults' % e)
        return None


def disable_nanite(m):
    """Box collision used for complex traces too, and no Nanite on props: their glTF materials (glass is translucent) aren't Nanite-compatible and
    rendered with the default material in cooked builds."""
    # Gun Torch shots are complex traces: let them hit the box collision.
    bs = m.get_editor_property('body_setup')
    if bs:
        bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    ns = m.get_editor_property('nanite_settings')
    if ns.get_editor_property('enabled'):
        ns.set_editor_property('enabled', False)
        m.set_editor_property('nanite_settings', ns)


def fix_existing():
    """IMPORT_HAVELI_ART=fix: only re-apply the mesh settings to already-imported props."""
    for p in eal.list_assets('/Game/Art/Props', recursive=True):
        m = eal.load_asset(p)
        if isinstance(m, unreal.StaticMesh):
            disable_nanite(m)
            eal.save_asset(m.get_path_name(), only_if_is_dirty=False)
            log('nanite off: %s (%s)' % (p, m.get_editor_property('nanite_settings').get_editor_property('enabled')))


def import_models():
    tasks = []
    for name in MODELS:
        dest = '/Game/Art/Props/' + name
        if eal.does_directory_exist(dest):
            eal.delete_directory(dest)
        f = os.path.join(PREP, name, name + '.gltf')
        if not os.path.exists(f):
            unreal.log_error('[import_haveli_art] missing ' + f)
            continue
        tasks.append(task(f, dest, 'SM_' + name, mesh_pipeline()))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for name in MODELS:
        meshes = [p for p in eal.list_assets('/Game/Art/Props/' + name, recursive=True)
                  if isinstance(eal.load_asset(p), unreal.StaticMesh)]
        if not meshes:
            unreal.log_error('[import_haveli_art] no mesh for ' + name)
            continue
        if len(meshes) > 1:
            log('WARNING several meshes for %s: %s' % (name, meshes))
        m = eal.load_asset(meshes[0])
        sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        sme.remove_collisions(m)
        sme.add_simple_collisions(m, unreal.ScriptCollisionShapeType.BOX)
        m.set_editor_property('light_map_resolution', 64)
        disable_nanite(m)
        eal.save_asset(m.get_path_name(), only_if_is_dirty=False)
        b = m.get_bounding_box()
        log('MESH %s %s min=(%.1f %.1f %.1f) max=(%.1f %.1f %.1f)' % (name, m.get_path_name(), b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))


if os.environ.get('IMPORT_HAVELI_ART') == 'fix':
    fix_existing()
else:
    import_textures()
    import_models()
log('done')
