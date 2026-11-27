"""
Creates simple procedural "aged" materials for the haveli in /Game/Materials.
No textures: colours are mixed with world-position noise, so no UVs are needed and
the pattern stays continuous across neighbouring grey-box cubes.

Skips any material that already exists, so reruns are safe (delete an asset to rebuild it).
Run inside the Unreal Editor (build_haveli.py runs this automatically).
"""
import unreal

FOLDER = "/Game/Materials"

mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty


class Graph:
    """Tiny helper around MaterialEditingLibrary so recipes below stay readable."""

    def __init__(self, mat):
        self.mat = mat
        self.y = 0

    def node(self, klass, **props):
        e = mel.create_material_expression(self.mat, klass, -800, self.y)
        self.y += 120
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, src, dst, dst_input=""):
        mel.connect_material_expressions(src, "", dst, dst_input)

    def color(self, r, g, b):
        return self.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1.0))

    def const(self, v):
        return self.node(unreal.MaterialExpressionConstant, r=v)

    def noise(self, scale, levels=3, stretch=None):
        """0..1 noise of world position. stretch=(sx, sy, sz) squashes the pattern (e.g. vertical streaks)."""
        n = self.node(unreal.MaterialExpressionNoise, scale=scale, levels=levels, quality=1,
                      output_min=0.0, output_max=1.0, turbulence=False)
        if stretch:
            wp = self.node(unreal.MaterialExpressionWorldPosition)
            m = self.node(unreal.MaterialExpressionMultiply)
            self.link(wp, m, "A")
            self.link(self.color(*stretch), m, "B")
            self.link(m, n, "Position")
        return n

    def mask(self, src, threshold, sharpness):
        """saturate((src - threshold) * sharpness)"""
        s = self.node(unreal.MaterialExpressionSubtract, const_b=threshold)
        self.link(src, s, "A")
        m = self.node(unreal.MaterialExpressionMultiply, const_b=sharpness)
        self.link(s, m, "A")
        sat = self.node(unreal.MaterialExpressionSaturate)
        self.link(m, sat)
        return sat

    def lerp(self, a, b, alpha):
        l = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, l, "A")
        self.link(b, l, "B")
        self.link(alpha, l, "Alpha")
        return l

    def mul(self, a, b):
        m = self.node(unreal.MaterialExpressionMultiply)
        self.link(a, m, "A")
        self.link(b, m, "B")
        return m

    def scale(self, a, k):
        m = self.node(unreal.MaterialExpressionMultiply, const_b=k)
        self.link(a, m, "A")
        return m

    def height_fade(self, top_cm):
        """1 at z=0 fading to 0 at z=top_cm (rising damp, dirt near the floor)."""
        wp = self.node(unreal.MaterialExpressionWorldPosition)
        z = self.node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
        self.link(wp, z)
        d = self.node(unreal.MaterialExpressionDivide, const_b=top_cm)
        self.link(z, d, "A")
        inv = self.node(unreal.MaterialExpressionOneMinus)
        self.link(d, inv)
        sat = self.node(unreal.MaterialExpressionSaturate)
        self.link(inv, sat)
        return sat

    def out(self, prop, expr):
        mel.connect_material_property(expr, "", prop)

    # ---- extra helpers (R17: ghost, decals, cobwebs, sky)

    def param(self, name, r, g, b):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                         default_value=unreal.LinearColor(r, g, b, 1.0))

    def sparam(self, name, v):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)

    def add(self, a, b):
        m = self.node(unreal.MaterialExpressionAdd)
        self.link(a, m, "A")
        self.link(b, m, "B")
        return m

    def maxe(self, a, b):
        m = self.node(unreal.MaterialExpressionMax)
        self.link(a, m, "A")
        self.link(b, m, "B")
        return m

    def one_minus(self, a):
        m = self.node(unreal.MaterialExpressionOneMinus)
        self.link(a, m)
        return m

    def chan(self, src, c):
        m = self.node(unreal.MaterialExpressionComponentMask, r=c == "r", g=c == "g", b=c == "b", a=False)
        self.link(src, m)
        return m

    def uv(self):
        return self.node(unreal.MaterialExpressionTextureCoordinate)

    def uv_radius(self):
        """Distance of the UV from the centre (0 centre, 0.5 at the edge midpoints)."""
        d = self.node(unreal.MaterialExpressionDistance)
        self.link(self.uv(), d, "A")
        self.link(self.node(unreal.MaterialExpressionConstant2Vector, r=0.5, g=0.5), d, "B")
        return d

    def lines(self, src, freq, width):
        """1 on thin lines where frac(src * freq) is near 0, width = fraction of a period."""
        f = self.node(unreal.MaterialExpressionFrac)
        self.link(self.scale(src, freq), f)
        s = self.node(unreal.MaterialExpressionSubtract, const_b=0.5)
        self.link(f, s, "A")
        a = self.node(unreal.MaterialExpressionAbs)
        self.link(s, a)
        return self.mask(self.scale(a, 2.0), 1.0 - width, 1.0 / max(width, 0.01))

    def noise_at(self, pos, scale, levels=2):
        n = self.node(unreal.MaterialExpressionNoise, scale=scale, levels=levels, quality=1,
                      output_min=0.0, output_max=1.0, turbulence=False)
        self.link(pos, n, "Position")
        return n


# ------------------------------------------------------------------ recipes

def old_plaster(g):
    # Big soft patches of sun-faded sand vs. warmer ochre.
    base = g.lerp(g.color(0.30, 0.20, 0.10), g.color(0.50, 0.38, 0.23), g.noise(0.0035, 4))
    # Vertical water streaks running down from the top.
    streak = g.mask(g.noise(0.025, 3, stretch=(1.0, 1.0, 0.06)), 0.5, 2.5)
    col = g.lerp(base, g.color(0.17, 0.13, 0.08), g.scale(streak, 0.6))
    # Patchy grime / mould, softer and sparser.
    blotch = g.mask(g.noise(0.009, 4), 0.64, 2.5)
    col = g.lerp(col, g.color(0.10, 0.08, 0.05), g.scale(blotch, 0.6))
    # Rising damp: darker band near the floor.
    col = g.lerp(col, g.color(0.16, 0.12, 0.07), g.scale(g.height_fade(110.0), 0.6))
    g.out(MP.MP_BASE_COLOR, col)
    g.out(MP.MP_ROUGHNESS, g.const(0.95))
    g.out(MP.MP_SPECULAR, g.const(0.3))


def old_stone(g):
    base = g.lerp(g.color(0.17, 0.15, 0.12), g.color(0.33, 0.29, 0.23), g.noise(0.008, 4))
    # Fine speckle for grit.
    col = g.lerp(base, g.color(0.42, 0.37, 0.30), g.scale(g.mask(g.noise(0.12, 2), 0.6, 3.0), 0.5))
    # Dark dirt patches.
    col = g.lerp(col, g.color(0.06, 0.05, 0.04), g.scale(g.mask(g.noise(0.004, 3), 0.55, 3.0), 0.7))
    g.out(MP.MP_BASE_COLOR, col)
    g.out(MP.MP_ROUGHNESS, g.lerp(g.const(0.95), g.const(0.7), g.noise(0.01, 2)))


def dark_stone(g):
    base = g.lerp(g.color(0.10, 0.09, 0.08), g.color(0.24, 0.21, 0.17), g.noise(0.015, 4))
    col = g.lerp(base, g.color(0.04, 0.04, 0.03), g.scale(g.mask(g.noise(0.03, 3, stretch=(1.0, 1.0, 0.15)), 0.55, 3.0), 0.7))
    g.out(MP.MP_BASE_COLOR, col)
    g.out(MP.MP_ROUGHNESS, g.const(0.9))


def old_wood(g):
    # Long grain: noise stretched along Z (doors/posts are vertical).
    grain = g.noise(0.06, 3, stretch=(1.0, 1.0, 0.05))
    base = g.lerp(g.color(0.03, 0.018, 0.01), g.color(0.10, 0.055, 0.03), grain)
    col = g.lerp(base, g.color(0.16, 0.13, 0.10), g.scale(g.mask(g.noise(0.01, 3), 0.7, 3.0), 0.35))  # dusty, dried patches
    g.out(MP.MP_BASE_COLOR, col)
    g.out(MP.MP_ROUGHNESS, g.const(0.8))


def brass(g):
    col = g.lerp(g.color(0.36, 0.25, 0.08), g.color(0.09, 0.075, 0.04), g.scale(g.mask(g.noise(0.1, 3), 0.40, 2.0), 0.85))  # tarnished (R17: was blowing out under the torch)
    g.out(MP.MP_BASE_COLOR, col)
    g.out(MP.MP_METALLIC, g.const(1.0))
    g.out(MP.MP_ROUGHNESS, g.lerp(g.const(0.45), g.const(0.8), g.noise(0.1, 2)))


def terracotta(g):
    base = g.lerp(g.color(0.30, 0.11, 0.05), g.color(0.50, 0.22, 0.10), g.noise(0.05, 3))
    col = g.lerp(base, g.color(0.45, 0.40, 0.34), g.scale(g.mask(g.noise(0.08, 3), 0.62, 3.0), 0.6))  # dried salt/dust
    g.out(MP.MP_BASE_COLOR, col)
    g.out(MP.MP_ROUGHNESS, g.const(0.95))


def rusty_iron(g):
    col = g.lerp(g.color(0.05, 0.045, 0.04), g.color(0.22, 0.09, 0.03), g.mask(g.noise(0.15, 3), 0.45, 2.0))
    g.out(MP.MP_BASE_COLOR, col)
    g.out(MP.MP_METALLIC, g.const(0.6))
    g.out(MP.MP_ROUGHNESS, g.const(0.85))


def dry_leaf(g):
    col = g.lerp(g.color(0.10, 0.06, 0.02), g.color(0.35, 0.20, 0.06), g.noise(0.3, 2))
    g.out(MP.MP_BASE_COLOR, col)
    g.out(MP.MP_ROUGHNESS, g.const(0.9))


def faded_cloth(g):
    # Dusty dark-red cloth (old bedding, curtains).
    col = g.lerp(g.color(0.12, 0.02, 0.02), g.color(0.30, 0.08, 0.05), g.noise(0.04, 3))
    col = g.lerp(col, g.color(0.25, 0.22, 0.18), g.scale(g.mask(g.noise(0.02, 3), 0.55, 3.0), 0.6))
    g.out(MP.MP_BASE_COLOR, col)
    g.out(MP.MP_ROUGHNESS, g.const(1.0))


# ------------------------------------------------------------------ R17 recipes

def ghost(g):
    """The Mirror Woman. Color / Glow / Rim parameters, set per part by AGhostBase."""
    colr = g.param("Color", 0.5, 0.5, 0.5)
    g.out(MP.MP_BASE_COLOR, colr)
    fres = g.node(unreal.MaterialExpressionFresnel, exponent=2.5, base_reflect_fraction=0.0)
    glow = g.add(g.sparam("Glow", 0.0), g.mul(fres, g.sparam("Rim", 0.0)))
    g.out(MP.MP_EMISSIVE_COLOR, g.mul(colr, glow))
    g.out(MP.MP_ROUGHNESS, g.const(0.9))
    g.out(MP.MP_SPECULAR, g.const(0.2))


def flame(g):
    g.mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # Hot yellow core fading to orange (by world height noise so each flame differs a little).
    col = g.lerp(g.color(1.0, 0.32, 0.05), g.color(1.0, 0.62, 0.22), g.noise(0.4, 2))
    g.out(MP.MP_EMISSIVE_COLOR, g.mul(col, g.sparam("Glow", 40.0)))


def cobweb(g):
    """Radial web on a plane (UV centre = web centre). Masked + two-sided."""
    g.mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    g.mat.set_editor_property("two_sided", True)
    uv = g.uv()
    c = g.node(unreal.MaterialExpressionSubtract, const_b=0.5)
    g.link(uv, c, "A")
    ang = g.node(unreal.MaterialExpressionArctangent2)
    g.link(g.chan(c, "g"), ang, "Y")
    g.link(g.chan(c, "r"), ang, "X")
    r = g.uv_radius()
    wobble = g.scale(g.noise(0.08, 2), 0.35)
    spokes = g.lines(ang, 9.0 / 6.2832, 0.10)
    rings = g.lines(g.add(r, g.scale(wobble, 0.06)), 10.0, 0.13)
    web = g.maxe(spokes, rings)
    inside = g.mask(g.one_minus(g.scale(r, 2.0)), 0.04, 12.0)      # fade out at the rim
    holes = g.mask(g.noise(0.035, 3), 0.28, 5.0)                     # torn patches
    g.out(MP.MP_OPACITY_MASK, g.mul(g.mul(web, inside), holes))
    g.out(MP.MP_BASE_COLOR, g.color(0.55, 0.55, 0.52))
    g.out(MP.MP_ROUGHNESS, g.const(0.7))


def muslin(g):
    """Torn, dirty off-white cloth for curtains in the arcade. Masked + two-sided."""
    g.mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    g.mat.set_editor_property("two_sided", True)
    base = g.lerp(g.color(0.13, 0.11, 0.085), g.color(0.27, 0.24, 0.20), g.noise(0.03, 3))
    base = g.lerp(base, g.color(0.09, 0.07, 0.05), g.scale(g.mask(g.noise(0.012, 3, stretch=(1.0, 1.0, 0.2)), 0.55, 3.0), 0.7))
    g.out(MP.MP_BASE_COLOR, base)
    g.out(MP.MP_ROUGHNESS, g.const(1.0))
    holes = g.mask(g.noise(0.045, 4), 0.33, 6.0)
    g.out(MP.MP_OPACITY_MASK, holes)


def make_decal(colour, rough, strength, streak, blotch_scale, fade_k=3.0):
    """Deferred decal: soft elliptical patch (UV radius) broken up by noise. streak = vertical drips."""
    def recipe(g):
        g.mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        g.mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
        fade = g.mask(g.one_minus(g.scale(g.uv_radius(), 2.0)), 0.0, fade_k)
        blot = g.mask(g.noise(blotch_scale, 4), 0.25, 1.4)
        if streak:
            drips = g.mask(g.noise(0.05, 3, stretch=(1.0, 1.0, 0.05)), 0.45, 3.0)
            blot = g.maxe(g.scale(blot, 0.8), g.mul(drips, g.one_minus(g.scale(g.uv_radius(), 1.4))))
        g.out(MP.MP_OPACITY, g.scale(g.mul(fade, blot), strength))
        g.out(MP.MP_BASE_COLOR, g.color(*colour))
        g.out(MP.MP_ROUGHNESS, g.const(rough))
    return recipe


def night_sky(g):
    """Unlit, unfogged sphere around the house: deep blue gradient, faint clouds, stars, the moon."""
    g.mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    g.mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    g.mat.set_editor_property("two_sided", True)
    g.mat.set_editor_property("use_translucency_vertex_fog", False)
    cam = g.node(unreal.MaterialExpressionCameraVectorWS)
    d = g.scale(cam, -1.0)                       # view direction
    z = g.node(unreal.MaterialExpressionSaturate)
    g.link(g.chan(d, "b"), z)
    sq = g.node(unreal.MaterialExpressionSquareRoot)
    g.link(z, sq)
    sky = g.lerp(g.color(0.030, 0.038, 0.060), g.color(0.004, 0.006, 0.014), sq)
    clouds = g.mask(g.noise_at(g.mul(d, g.color(600.0, 600.0, 1500.0)), 0.004, 5), 0.45, 2.0)
    sky = g.lerp(sky, g.color(0.040, 0.044, 0.055), g.scale(clouds, 0.8))
    stars = g.mask(g.noise_at(g.scale(d, 9000.0), 0.03, 1), 0.93, 30.0)
    stars = g.mul(stars, g.mask(g.noise_at(g.scale(d, 3000.0), 0.02, 1), 0.5, 4.0))   # thin them out unevenly
    sky = g.add(sky, g.mul(g.scale(stars, 0.25), g.one_minus(clouds)))
    # Moon: same direction as the Moonlight actor in build_haveli.py (pitch -58, yaw 35).
    import math
    p, y = math.radians(58.0), math.radians(35.0)
    to_moon = g.color(-math.cos(p) * math.cos(y), -math.cos(p) * math.sin(y), math.sin(p))
    dp = g.node(unreal.MaterialExpressionDotProduct)
    g.link(d, dp, "A")
    g.link(to_moon, dp, "B")
    disc = g.mask(dp, 0.99965, 20000.0)
    dsat = g.node(unreal.MaterialExpressionSaturate)
    g.link(dp, dsat)
    halo = g.node(unreal.MaterialExpressionPower, const_exponent=300.0)
    g.link(dsat, halo, "Base")
    moon = g.add(g.mul(disc, g.color(3.0, 3.1, 3.4)), g.mul(halo, g.color(0.10, 0.12, 0.18)))
    g.out(MP.MP_EMISSIVE_COLOR, g.add(sky, moon))
    g.out(MP.MP_OPACITY, g.const(1.0))


MATERIALS = {
    "M_OldPlaster": old_plaster,
    "M_OldStone": old_stone,
    "M_DarkStone": dark_stone,
    "M_OldWood": old_wood,
    "M_Brass": brass,
    "M_Terracotta": terracotta,
    "M_RustyIron": rusty_iron,
    "M_DryLeaf": dry_leaf,
    "M_FadedCloth": faded_cloth,
    "M_Ghost": ghost,
    "M_Flame": flame,
    "M_Cobweb": cobweb,
    "M_Muslin": muslin,
    "M_DecalWaterStain": make_decal((0.035, 0.025, 0.014), 0.55, 0.6, True, 0.012),
    "M_DecalSoot": make_decal((0.006, 0.005, 0.005), 0.95, 0.75, False, 0.02),
    "M_DecalDust": make_decal((0.30, 0.27, 0.22), 1.0, 0.55, False, 0.03, fade_k=1.6),
    "M_NightSky": night_sky,
}


def create_materials():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name, recipe in MATERIALS.items():
        path = f"{FOLDER}/{name}"
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log(f"Material exists, skipping: {path}")
            continue
        mat = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
        recipe(Graph(mat))
        mel.recompile_material(mat)
        unreal.EditorAssetLibrary.save_loaded_asset(mat)
        unreal.log(f"Created material: {path}")


if __name__ == "__main__":
    create_materials()
