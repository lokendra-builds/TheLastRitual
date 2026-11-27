"""World-aligned PBR materials for the haveli's big surfaces (R21), from the CC0 Poly Haven sets that
Scripts/import_haveli_art.py imports to /Game/Art/Textures/<name>/.

The level is built from scaled engine cubes, so mesh UVs stretch. M_PBR_WorldAligned projects the
textures from world position instead: it picks the dominant axis of the vertex normal (floor/ceiling,
X-facing wall, Y-facing wall) and samples each texture ONCE with that plane's world coordinates (cheap:
3 texture samples, no tri-planar blend; the level is axis-aligned so a hard switch is invisible).
The DirectX normal map is turned into a world-space perturbation of the vertex normal.
On top: Tint (keeps the warm haveli palette), soft world-noise grime and macro variation (hides tiling),
rising damp near the floor. Material instances per surface: MI_Plaster, MI_FloorTile, MI_Paving,
MI_Wood, MI_Sandstone, MI_WellStone in /Game/Art/Materials.

Safe to rerun: the master graph is rebuilt in place and the instances' parameters are re-applied.
Run inside the editor (build_haveli.py runs it).
"""
import os
import runpy

import unreal

SCRIPTS = os.path.join(unreal.Paths.project_dir(), "Scripts")
_cm = runpy.run_path(os.path.join(SCRIPTS, "create_materials.py"), run_name="create_materials_lib")
Graph = _cm["Graph"]
mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
eal = unreal.EditorAssetLibrary

FOLDER = "/Game/Art/Materials"
MASTER = "M_PBR_WorldAligned"
TEX = "/Game/Art/Textures"

# name: (texture set, tile size cm, tint rgb, scalar overrides)
INSTANCES = {
    "MI_Plaster": ("rough_plaster_broken", 170.0, (0.92, 0.74, 0.52), {"DampAmount": 0.65, "Grime": 0.55, "NormalStrength": 1.0}),
    "MI_FloorTile": ("worn_tile_floor", 190.0, (0.80, 0.70, 0.58), {"DampAmount": 0.0, "Grime": 0.45, "NormalStrength": 0.9}),
    "MI_Paving": ("red_sandstone_pavement", 230.0, (0.95, 0.80, 0.66), {"DampAmount": 0.0, "Grime": 0.4, "NormalStrength": 1.0}),
    "MI_Wood": ("rough_wood", 90.0, (0.30, 0.19, 0.12), {"DampAmount": 0.0, "Grime": 0.25, "NormalStrength": 1.2}),
    "MI_Sandstone": ("sandstone_cracks", 160.0, (0.50, 0.40, 0.31), {"DampAmount": 0.45, "Grime": 0.45, "NormalStrength": 1.0}),
    "MI_WellStone": ("rustic_stone_wall", 130.0, (0.62, 0.55, 0.46), {"DampAmount": 0.6, "Grime": 0.35, "NormalStrength": 1.1}),
}


def link(src, dst, src_out="", dst_in=""):
    mel.connect_material_expressions(src, src_out, dst, dst_in)


def build_master(mat):
    g = Graph(mat)
    mat.set_editor_property("tangent_space_normal", False)   # Normal below is world space

    wp = g.node(unreal.MaterialExpressionWorldPosition)
    n = g.node(unreal.MaterialExpressionVertexNormalWS)
    an = g.node(unreal.MaterialExpressionAbs)
    g.link(n, an)
    ax, ay, az = g.chan(an, "r"), g.chan(an, "g"), g.chan(an, "b")
    # zdom = 1 on floors / ceilings, ydom = 1 on walls facing +-Y.
    # (ties, e.g. the 45-degree octagonal pillar faces, go to Y / Z instead of a smeared blend)
    zdom = g.mask(g.add(az, g.scale(g.maxe(ax, ay), -1.0)), -0.002, 1000.0)
    ydom = g.mask(g.add(ay, g.scale(ax, -1.0)), -0.002, 1000.0)

    wx, wy, wz = g.chan(wp, "r"), g.chan(wp, "g"), g.chan(wp, "b")
    wall_u = g.lerp(wy, wx, ydom)
    wall_uv = g.node(unreal.MaterialExpressionAppendVector)
    g.link(wall_u, wall_uv, "A")
    g.link(g.scale(wz, -1.0), wall_uv, "B")         # image up = world up
    floor_uv = g.node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False)
    g.link(wp, floor_uv)
    uv_cm = g.lerp(wall_uv, floor_uv, zdom)
    uv = g.node(unreal.MaterialExpressionDivide)
    g.link(uv_cm, uv, "A")
    g.link(g.sparam("TileCM", 150.0), uv, "B")

    def tex_param(name, sampler, default):
        e = g.node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name=name, sampler_type=sampler)
        t = unreal.load_asset(default)
        if t:
            e.set_editor_property("texture", t)
        link(uv, e, "", "UVs")
        return e

    base_set = f"{TEX}/rough_plaster_broken/T_rough_plaster_broken"
    d = tex_param("Diffuse", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, base_set + "_D")
    nm = tex_param("Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, base_set + "_N")
    arm = tex_param("ARM", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, base_set + "_ARM")

    # ---- base colour: texture * tint, macro variation, grime blotches, rising damp
    col = g.mul(d, g.param("Tint", 1.0, 1.0, 1.0))
    macro = g.lerp(g.const(1.0), g.lerp(g.const(0.72), g.const(1.12), g.noise(0.0022, 2)), g.sparam("MacroVariation", 1.0))
    col = g.mul(col, macro)
    blotch = g.mask(g.noise(0.0075, 3), 0.58, 2.2)
    col = g.lerp(col, g.scale(col, 0.32), g.mul(blotch, g.sparam("Grime", 0.5)))
    z = g.chan(wp, "b")
    dz = g.node(unreal.MaterialExpressionDivide)
    g.link(z, dz, "A")
    g.link(g.sparam("DampHeight", 110.0), dz, "B")
    damp = g.node(unreal.MaterialExpressionSaturate)
    g.link(g.one_minus(dz), damp)
    col = g.lerp(col, g.scale(col, 0.5), g.mul(damp, g.sparam("DampAmount", 0.5)))
    g.out(MP.MP_BASE_COLOR, col)

    # ---- world-space normal: vertex normal + projected tangent-space xy
    tx = g.node(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
    link(nm, tx)
    ty = g.node(unreal.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False)
    link(nm, ty)
    zero = g.const(0.0)
    nty = g.scale(ty, -1.0)

    def vec3(a, b, c):
        ab = g.node(unreal.MaterialExpressionAppendVector)
        g.link(a, ab, "A")
        g.link(b, ab, "B")
        abc = g.node(unreal.MaterialExpressionAppendVector)
        g.link(ab, abc, "A")
        g.link(c, abc, "B")
        return abc

    p_xwall = vec3(zero, tx, nty)     # wall facing X: u = world Y, image up = +Z
    p_ywall = vec3(tx, zero, nty)     # wall facing Y: u = world X
    p_floor = vec3(tx, ty, zero)      # floor: u = X, v = Y (DX green points +v)
    pert = g.lerp(g.lerp(p_xwall, p_ywall, ydom), p_floor, zdom)
    wn = g.add(n, g.mul(pert, g.sparam("NormalStrength", 1.0)))
    nrm = g.node(unreal.MaterialExpressionNormalize)
    g.link(wn, nrm)
    g.out(MP.MP_NORMAL, nrm)

    # ---- ARM: AO / roughness
    ao = g.node(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
    link(arm, ao)
    rough = g.node(unreal.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False)
    link(arm, rough)
    r = g.node(unreal.MaterialExpressionSaturate)
    g.link(g.add(g.mul(rough, g.sparam("RoughnessScale", 1.0)), g.sparam("RoughnessAdd", 0.05)), r)
    g.out(MP.MP_ROUGHNESS, r)
    g.out(MP.MP_AMBIENT_OCCLUSION, g.lerp(g.const(1.0), ao, g.sparam("AOStrength", 1.0)))
    g.out(MP.MP_SPECULAR, g.sparam("Specular", 0.35))


def master():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    path = f"{FOLDER}/{MASTER}"
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(MASTER, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    build_master(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    unreal.log(f"[pbr] master {path}")
    return mat


def instances(parent):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name, (tex, tile, tint, scalars) in INSTANCES.items():
        path = f"{FOLDER}/{name}"
        if eal.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        ok = True
        for p, suffix in (("Diffuse", "D"), ("Normal", "N"), ("ARM", "ARM")):
            t = unreal.load_asset(f"{TEX}/{tex}/T_{tex}_{suffix}")
            if not t:
                unreal.log_error(f"[pbr] missing texture {tex} {suffix}")
                ok = False
                continue
            mel.set_material_instance_texture_parameter_value(mi, p, t)
        mel.set_material_instance_scalar_parameter_value(mi, "TileCM", tile)
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
        for k, v in scalars.items():
            mel.set_material_instance_scalar_parameter_value(mi, k, v)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        unreal.log(f"[pbr] instance {path} ({'ok' if ok else 'MISSING TEXTURES'})")


def lantern_glow():
    """Warm glowing glass for the lanterns' glass slot (unlit, translucent)."""
    path = f"{FOLDER}/M_LanternGlow"
    if eal.does_asset_exist(path):
        return
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset("M_LanternGlow", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    g = Graph(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)
    fres = g.node(unreal.MaterialExpressionFresnel, exponent=1.5, base_reflect_fraction=0.0)
    col = g.lerp(g.color(1.0, 0.55, 0.20), g.color(0.6, 0.22, 0.05), fres)
    g.out(MP.MP_EMISSIVE_COLOR, g.mul(col, g.sparam("Glow", 6.0)))
    g.out(MP.MP_OPACITY, g.lerp(g.const(0.55), g.const(0.25), fres))
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    unreal.log(f"[pbr] created {path}")


def create_pbr_materials():
    if not eal.does_directory_exist(TEX):
        unreal.log_warning("[pbr] /Game/Art/Textures missing: run Scripts/import_haveli_art.py first")
        return
    instances(master())
    lantern_glow()


if __name__ == "__main__":
    create_pbr_materials()
