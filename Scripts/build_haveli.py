"""
Builds the Night 1 haveli: /Game/Maps/Haveli_Night1

A free-roam ground floor around an open moonlit courtyard with the well.
Rooms open onto a covered veranda (pillars and cusped arches on the courtyard side).
Everything is built from engine basic shapes with procedural "aged" materials
(Scripts/create_materials.py): weathered plaster, worn stone, dark wood, brass, terracotta.

Run inside the Unreal Editor (start.sh does this for you):
    Tools > Execute Python Script... > Scripts/build_haveli.py

Top view (cm). Courtyard |x|,|y| < 600 (open sky). Veranda 600..900 (roofed).
Rooms 900..1400. Outer wall at 1400.

        +-----------+-----------+-----------+
        |   Store   |  Prayer   |  Bedroom  |  north (mirror, hair)
        +-----+-----+--door-----+-----+-----+
        |Dine |   veranda              | Bed 2|  (mirror, diary)
        +-----+   .  courtyard  .     +------+
        |Store2|  .    (well)    .    | Clock|
        +-----+-----+-----+-----+-----+------+
        |  Kitchen  | Entrance  |   Guest   |  south (salt / gate / mirror)
        +-----------+--[gate]---+-----------+
"""
import math
import os
import random
import runpy

import unreal

SCRIPTS = os.path.join(unreal.Paths.project_dir(), "Scripts")
runpy.run_path(os.path.join(SCRIPTS, "create_ghosts.py"), run_name="__main__")
runpy.run_path(os.path.join(SCRIPTS, "create_materials.py"), run_name="__main__")
# R21: world-aligned PBR materials from the CC0 Poly Haven textures (needs Scripts/import_haveli_art.py run once).
runpy.run_path(os.path.join(SCRIPTS, "create_pbr_materials.py"), run_name="__main__")

# HAVELI_MAP lets you build a scratch copy (e.g. /Game/Maps/Haveli_Dev) without touching the real map.
MAP_PATH = os.environ.get("HAVELI_MAP", "/Game/Maps/Haveli_Night1")

H = 350.0          # ceiling height
T = 25.0           # wall thickness
C = 600.0          # courtyard half-size
V = 900.0          # veranda outer edge / inner room wall
O = 1400.0         # outer wall
DOOR_W, DOOR_H = 100.0, 210.0
WIN_W, WIN_Z0, WIN_Z1 = 80.0, 105.0, 210.0   # barred jharokha windows in the outer walls

# Lighting knobs. EXPOSURE_EV100: lower = brighter picture.
# The player's torch is a radial-falloff light of about 12 "lux" near (TorchComponent), the moon is
# the brightest thing in the house; lamps are small warm pools. Tune these three together.
MOON_LUX = 3.0
MOON_PITCH, MOON_YAW = -58.0, 35.0      # shafts through the south/west veranda roof holes (see M_NightSky)
LAMP_CANDELA = 4.0
SKY_FILL = 1.0
EXPOSURE_EV100 = 1.0

level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    raise RuntimeError(f"{MAP_PATH} already exists. Delete it first if you want to rebuild it.")

level_sub.new_level(MAP_PATH)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
cylinder = unreal.load_asset("/Engine/BasicShapes/Cylinder")
sphere = unreal.load_asset("/Engine/BasicShapes/Sphere")
plane = unreal.load_asset("/Engine/BasicShapes/Plane")


def material(name):
    m = unreal.load_asset(f"/Game/Materials/{name}")
    if m is None:
        unreal.log_warning(f"Material /Game/Materials/{name} missing, using the default.")
    return m


def art_material(name, fallback):
    """/Game/Art/Materials/<name> (Poly Haven PBR, world-aligned) or the procedural fallback."""
    path = f"/Game/Art/Materials/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    unreal.log_warning(f"{path} missing (run Scripts/import_haveli_art.py), using {fallback}.")
    return material(fallback)


PLASTER = art_material("MI_Plaster", "M_OldPlaster")
STONE = material("M_OldStone")                       # rubble / grit
FLOOR = art_material("MI_FloorTile", "M_OldStone")   # room + veranda floors
PAVING = art_material("MI_Paving", "M_OldStone")     # courtyard + outside the gate
DARK = art_material("MI_Sandstone", "M_DarkStone")   # pillars, plinths, copings, altar, counter
WELLSTONE = art_material("MI_WellStone", "M_DarkStone")
WOOD = art_material("MI_Wood", "M_OldWood")
LANTERN_GLOW = unreal.load_asset("/Game/Art/Materials/M_LanternGlow")
BRASS = material("M_Brass")
CLAY = material("M_Terracotta")
IRON = material("M_RustyIron")
LEAF = material("M_DryLeaf")
CLOTH = material("M_FadedCloth")
FLAME = material("M_Flame")
COBWEB = material("M_Cobweb")
MUSLIN = material("M_Muslin")
SKY = material("M_NightSky")
DECAL_STAIN = material("M_DecalWaterStain")
DECAL_SOOT = material("M_DecalSoot")
DECAL_DUST = material("M_DecalDust")

rng = random.Random(1987)
decor_count = 0


def cls(name):
    return unreal.load_class(None, f"/Script/TheLastRitual.{name}")


def spawn(klass, loc, yaw=0.0, label=None, folder=None):
    a = actor_sub.spawn_actor_from_class(klass, unreal.Vector(*loc), unreal.Rotator(0, 0, yaw))
    if label:
        a.set_actor_label(label)
    if folder:
        a.set_folder_path(folder)
    return a


def _finish_mesh(a, mesh, mat, collide):
    smc = a.static_mesh_component
    smc.set_static_mesh(mesh or cube)
    if mat:
        smc.set_material(0, mat)
    if not collide:
        smc.set_collision_profile_name("NoCollision")
        try:
            smc.set_can_ever_affect_navigation(False)
        except Exception:
            pass
    return a


def box(label, x0, x1, y0, y1, z0, z1, folder="Greybox", mesh=None, mat=PLASTER, collide=True):
    if x1 - x0 <= 0.5 or y1 - y0 <= 0.5 or z1 - z0 <= 0.5:
        return None
    a = spawn(unreal.StaticMeshActor, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), label=label, folder=folder)
    a.set_actor_scale3d(unreal.Vector((x1 - x0) / 100, (y1 - y0) / 100, (z1 - z0) / 100))
    return _finish_mesh(a, mesh, mat, collide)


def piece(label, center, size, mat, rot=(0.0, 0.0, 0.0), folder="Decor", mesh=None, collide=False):
    """Decor mesh centred at `center`, size in cm (before rotation), rot = (pitch, yaw, roll)."""
    global decor_count
    decor_count += 1
    a = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*center),
                                         unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    a.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, size[2] / 100))
    return _finish_mesh(a, mesh, mat, collide)


def wall_piece(label, axis, c, a0, a1, z0, z1, thick=T, mat=PLASTER, folder="Greybox", collide=True):
    """Box on a wall line: axis 'x' = wall runs along X at y=c, 'y' = along Y at x=c."""
    if axis == "x":
        return box(label, a0, a1, c - thick / 2, c + thick / 2, z0, z1, folder=folder, mat=mat, collide=collide)
    return box(label, c - thick / 2, c + thick / 2, a0, a1, z0, z1, folder=folder, mat=mat, collide=collide)


def arch_half(label, axis, c, spring, apex, depth, thick, mat, cusps=(0.5,), cusp_size=14.0, folder="Arches"):
    """One side of a pointed (cusped) arch in the vertical plane of a wall line.
    spring/apex = (a, z): a is the coordinate along the wall. The slab's lower edge is the arch line,
    it grows upwards by `thick` (hide the top inside a lintel/fascia). Small diamonds make the cusps."""
    (a0, z0), (a1, z1) = spring, apex
    sign = 1.0 if a1 > a0 else -1.0
    run, rise = abs(a1 - a0), z1 - z0
    ang = math.degrees(math.atan2(rise, run))
    length = math.hypot(run, rise)
    sa, ca = math.sin(math.radians(ang)), math.cos(math.radians(ang))
    # Normal in the (a, z) plane, pointing up/inwards.
    na, nz = -sa * sign * thick / 2, ca * thick / 2
    am, zm = (a0 + a1) / 2 + na, (z0 + z1) / 2 + nz
    if axis == "x":
        center, yaw = (am, c, zm), (0.0 if sign > 0 else 180.0)
    else:
        center, yaw = (c, am, zm), (90.0 if sign > 0 else -90.0)
    piece(label, center, (length, depth, thick), mat, (ang, yaw, 0.0), folder=folder)
    for i, f in enumerate(cusps):
        pa, pz = a0 + (a1 - a0) * f, z0 + rise * f
        pc = (pa, c, pz) if axis == "x" else (c, pa, pz)
        piece(f"{label}_cusp{i}", pc, (cusp_size, depth * 0.9, cusp_size), mat, (45.0 + ang, yaw, 0.0), folder=folder)


def plinth(label, axis, c, a0, a1):
    """Raised plinth band at the foot of a wall (both faces)."""
    if a1 - a0 > 2:
        wall_piece(label, axis, c, a0, a1, 0, 22, thick=T + 8, mat=DARK, folder="Plinth", collide=False)


def door_frame(label, axis, c, g):
    w = DOOR_W / 2
    wall_piece(f"{label}_jambL", axis, c, g - w - 10, g - w, 0, DOOR_H + 12, thick=T + 6, mat=WOOD, folder="DoorFrames", collide=False)
    wall_piece(f"{label}_jambR", axis, c, g + w, g + w + 10, 0, DOOR_H + 12, thick=T + 6, mat=WOOD, folder="DoorFrames", collide=False)
    wall_piece(f"{label}_head", axis, c, g - w - 16, g + w + 16, DOOR_H, DOOR_H + 14, thick=T + 8, mat=WOOD, folder="DoorFrames", collide=False)


def window(label, axis, c, w):
    """Barred jharokha: wall below/above the opening, pointed wooden head, iron bars, stone sill."""
    hw = WIN_W / 2
    wall_piece(f"{label}_sill", axis, c, w - hw, w + hw, 0, WIN_Z0)
    wall_piece(f"{label}_head", axis, c, w - hw, w + hw, WIN_Z1, H)
    wall_piece(f"{label}_ledge", axis, c, w - hw - 12, w + hw + 12, WIN_Z0 - 8, WIN_Z0 + 2, thick=T + 16, mat=DARK, folder="Windows", collide=False)
    wall_piece(f"{label}_jambL", axis, c, w - hw - 9, w - hw, WIN_Z0, WIN_Z1 + 10, thick=T + 6, mat=WOOD, folder="Windows", collide=False)
    wall_piece(f"{label}_jambR", axis, c, w + hw, w + hw + 9, WIN_Z0, WIN_Z1 + 10, thick=T + 6, mat=WOOD, folder="Windows", collide=False)
    arch_half(f"{label}_archL", axis, c, (w - hw, WIN_Z1 - 32), (w, WIN_Z1 - 4), T - 3, 22, WOOD, cusps=(0.45,), cusp_size=8, folder="Windows")
    arch_half(f"{label}_archR", axis, c, (w + hw, WIN_Z1 - 32), (w, WIN_Z1 - 4), T - 3, 22, WOOD, cusps=(0.45,), cusp_size=8, folder="Windows")
    for i, off in enumerate((-27, -9, 9, 27)):
        wall_piece(f"{label}_bar{i}", axis, c, w + off - 1.5, w + off + 1.5, WIN_Z0, WIN_Z1, thick=3, mat=IRON, folder="Windows")
    wall_piece(f"{label}_rail", axis, c, w - hw, w + hw, 150, 153, thick=4, mat=IRON, folder="Windows", collide=False)


def _wall(label, axis, c, a0, a1, gaps=(), windows=()):
    openings = sorted([(g, DOOR_W / 2, "door") for g in gaps] + [(w, WIN_W / 2, "win") for w in windows])
    edges = [a0]
    for g, hw, _ in openings:
        edges += [g - hw, g + hw]
    edges.append(a1)
    door_edges = {g - DOOR_W / 2 for g in gaps} | {g + DOOR_W / 2 for g in gaps}
    for i in range(0, len(edges), 2):
        s0, s1 = edges[i], edges[i + 1]
        wall_piece(f"{label}_{i // 2}", axis, c, s0, s1, 0, H)
        # Keep the plinth clear of door swings.
        plinth(f"{label}_plinth_{i // 2}", axis, c, s0 + (14 if s0 in door_edges else 0), s1 - (14 if s1 in door_edges else 0))
    for g, hw, kind in openings:
        if kind == "door":   # lintel above each door
            wall_piece(f"{label}_lintel_{int(g)}", axis, c, g - hw, g + hw, DOOR_H, H)
            door_frame(f"{label}_frame_{int(g)}", axis, c, g)
        else:
            window(f"{label}_win_{int(g)}", axis, c, g)


def wall_x(label, y, x0, x1, gaps=(), windows=()):
    """Wall running along X at y, from x0 to x1, with door gaps centred at the given x values."""
    _wall(label, "x", y, x0, x1, gaps, windows)


def wall_y(label, x, y0, y1, gaps=(), windows=()):
    """Wall running along Y at x."""
    _wall(label, "y", x, y0, y1, gaps, windows)


def slab_with_holes(label, x0, x1, y0, y1, z0, z1, holes=(), mat=PLASTER):
    """Roof slab split into boxes around rectangular holes (hx0, hx1, hy0, hy1)."""
    xs = sorted({x0, x1} | {v for h in holes for v in (h[0], h[1]) if x0 < v < x1})
    n = 0
    for xa, xb in zip(xs, xs[1:]):
        cuts = sorted((h[2], h[3]) for h in holes if h[0] <= xa and h[1] >= xb)
        ya = y0
        for hy0, hy1 in cuts + [(y1, y1)]:
            box(f"{label}_{n}", xa, xb, ya, hy0, z0, z1, mat=mat)
            n += 1
            ya = hy1


# ------------------------------------------------------------------ real props (R21, CC0 Poly Haven, /Game/Art/Props)

PROPS = "/Game/Art/Props"
breakable_cls = cls("BreakableProp")
prop_count = 0
breakable_count = 0


def prop_mesh(name):
    path = f"{PROPS}/{name}/{name}/StaticMeshes/SM_{name}"
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log_warning(f"Prop {path} missing (run Scripts/import_haveli_art.py).")
        return None
    return unreal.load_asset(path)


def _prop_place(mesh, x, y, z, yaw, scale, height):
    """Uniform scale (or fit to `height` cm) and the actor location that puts the mesh's
    bottom-centre at (x, y, z) after turning it by yaw. Prop fronts face +Y before the turn."""
    b = mesh.get_bounding_box()
    s = height / max(b.max.z - b.min.z, 0.1) if height else scale
    cx, cy = (b.min.x + b.max.x) / 2 * s, (b.min.y + b.max.y) / 2 * s
    c, sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return s, (x - (cx * c - cy * sn), y - (cx * sn + cy * c), z - b.min.z * s)


def prop(label, name, x, y, z=0.0, yaw=0.0, scale=1.0, height=None, folder="Props", collide=True, tilt=(0.0, 0.0)):
    """Static decor prop. tilt = (pitch, roll)."""
    global prop_count
    mesh = prop_mesh(name)
    if mesh is None:
        return None
    s, loc = _prop_place(mesh, x, y, z, yaw, scale, height)
    a = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc),
                                         unreal.Rotator(roll=tilt[1], pitch=tilt[0], yaw=yaw))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    a.set_actor_scale3d(unreal.Vector(s, s, s))
    smc = a.static_mesh_component
    smc.set_static_mesh(mesh)
    if not collide:
        smc.set_collision_profile_name("NoCollision")
    prop_count += 1
    return a


def breakable(label, name, x, y, z=0.0, yaw=0.0, scale=1.0, height=None, kind=None, shards=12,
              light=None, glow_slot=None, folder="Breakables"):
    """A BreakableProp (the Gun Torch smashes it). light = (flame height above the base in cm, candela, radius)."""
    global breakable_count
    mesh = prop_mesh(name)
    if mesh is None or breakable_cls is None:
        return None
    s, loc = _prop_place(mesh, x, y, z, yaw, scale, height)
    a = actor_sub.spawn_actor_from_class(breakable_cls, unreal.Vector(*loc), unreal.Rotator(0, 0, yaw))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    a.set_actor_scale3d(unreal.Vector(s, s, s))
    m = a.get_editor_property("mesh")
    m.set_static_mesh(mesh)
    if glow_slot is not None and LANTERN_GLOW:
        m.set_material(glow_slot, LANTERN_GLOW)
    a.set_editor_property("kind", kind if kind is not None else unreal.BreakableKind.CLAY_POT)
    a.set_editor_property("shard_count", shards)
    if light:
        lz, cd, rad = light
        a.set_editor_property("has_light", True)
        m.set_cast_shadow(False)        # the light sits inside the lantern
        lc = a.get_editor_property("light")
        lc.set_mobility(unreal.ComponentMobility.MOVABLE)
        lc.set_world_location(unreal.Vector(x, y, z + lz), False, False)
        for prop_name, value in (("intensity_units", unreal.LightUnits.CANDELAS), ("intensity", cd),
                                 ("attenuation_radius", rad), ("source_radius", 2.0),
                                 ("light_color", unreal.Color(r=255, g=140, b=48, a=255)),
                                 ("volumetric_scattering_intensity", 1.5)):
            try:
                lc.set_editor_property(prop_name, value)
            except Exception as exc:
                unreal.log_warning(f"Breakable light {prop_name}: {exc}")
    breakable_count += 1
    return a


BK = getattr(unreal, "BreakableKind", None)
POT = BK.CLAY_POT if BK else None
CRATE = BK.CRATE if BK else None
LANTERN = BK.LANTERN if BK else None
GLASS = BK.GLASS if BK else None

door_cls = cls("HouseDoor")


def _wood_door(d):
    try:
        d.get_editor_property("door_mesh").set_material(0, WOOD)
    except Exception as exc:
        unreal.log_warning(f"Door material: {exc}")


def door_in_x_wall(label, x, y, locked=False):
    # Hinge on the left edge; door lies along local +Y, yaw -90 turns it to world +X.
    d = spawn(door_cls, (x - DOOR_W / 2, y, 0), -90, label, "Doors")
    d.set_editor_property("locked", locked)
    _wood_door(d)
    return d


def door_in_y_wall(label, x, y, locked=False):
    d = spawn(door_cls, (x, y - DOOR_W / 2, 0), 0, label, "Doors")
    d.set_editor_property("locked", locked)
    _wood_door(d)
    return d


# ------------------------------------------------------------------ structure

box("Ground", -O - T, O + T, -O - T, O + T, -T, 0, mat=FLOOR)
# Open-air sandstone paving: the courtyard (a hair proud of the floor) and the strip outside the gate.
box("Courtyard_Paving", -C, C, -C, C, -2, 0.4, mat=PAVING)
box("Ground_Outside", -O - T, O + T, -O - T - 400, -O - T, -T, 0, mat=PAVING)

# Roof over veranda + rooms (courtyard stays open to the sky). Broken patches let moonlight in.
HOLE_VERANDA_S = (-300, -160, -880, -775)
HOLE_VERANDA_W = (-880, -780, 150, 280)
HOLE_CLOCK = (1250, 1370, -870, -770)
slab_with_holes("Roof_N", -O - T, O + T, C, O + T, H, H + T)
slab_with_holes("Roof_S", -O - T, O + T, -O - T, -C, H, H + T, holes=(HOLE_VERANDA_S, HOLE_CLOCK))
slab_with_holes("Roof_E", C, O + T, -C, C, H, H + T)
slab_with_holes("Roof_W", -O - T, -C, -C, C, H, H + T, holes=(HOLE_VERANDA_W,))
# Roof terrace parapet around the courtyard opening.
for name, a in (("N", (-C, C, C, C + T)), ("S", (-C, C, -C - T, -C)), ("E", (C, C + T, -C, C)), ("W", (-C - T, -C, -C, C))):
    box(f"Parapet_{name}", a[0], a[1], a[2], a[3], H + T, H + T + 90)
    box(f"Parapet_{name}_coping", a[0] - 4, a[1] + 4, a[2] - 4, a[3] + 4, H + T + 90, H + T + 98, mat=DARK, collide=False)

# Outer walls (front gate gap at x=0 on the south). Barred windows on the moonlit (south/west) sides.
wall_x("Outer_N", O + T / 2, -O - T, O + T, windows=(-1000,))
wall_x("Outer_S", -O - T / 2, -O - T, O + T, gaps=(0,), windows=(-700, 1250))
wall_y("Outer_E", O + T / 2, -O, O)
wall_y("Outer_W", -O - T / 2, -O, O, windows=(-80, 450))

# Inner walls between veranda and rooms, one door per room.
wall_x("Inner_N", V, -O, O, gaps=(-700, 0, 700))
wall_x("Inner_S", -V, -O, O, gaps=(-700, 0, 700))
wall_y("Inner_E", V, -V, V, gaps=(-450, 450))
wall_y("Inner_W", -V, -V, V, gaps=(-450, 450))

# Partitions between rooms.
for x in (-470, 470):
    wall_y(f"Part_N_{x}", x, V, O)
    wall_y(f"Part_S_{x}", x, -O, -V)
wall_x("Part_E", 0, V, O)
wall_x("Part_W", 0, -O, -V)

# Courtyard pillars: stepped base, octagonal shaft, bracket capital.
for i, p in enumerate((-C, -C / 2, 0, C / 2, C)):
    for name, (px, py) in (("N", (p, C)), ("S", (p, -C)), ("E", (C, p)), ("W", (-C, p))):
        if name in ("E", "W") and abs(p) == C:
            continue  # corners already placed
        box(f"Pillar_{name}_{i}", px - 17, px + 17, py - 17, py + 17, 0, H, mat=DARK)
        piece(f"Pillar_{name}_{i}_oct", (px, py, 130), (34, 34, 190), DARK, (0, 45, 0), folder="Pillars")
        piece(f"Pillar_{name}_{i}_base", (px, py, 16), (60, 60, 32), DARK, folder="Pillars", collide=True)
        piece(f"Pillar_{name}_{i}_cap", (px, py, 235), (48, 48, 20), DARK, (0, 45, 0), folder="Pillars")
        piece(f"Pillar_{name}_{i}_bracket", (px, py, 260), (62, 62, 30), DARK, folder="Pillars")
        piece(f"Pillar_{name}_{i}_abacus", (px, py, 288), (40, 40, 26), DARK, folder="Pillars")

# Cusped pointed arches between the pillars, under a deep fascia and a thin chhajja (eave).
for side, axis, c in (("N", "x", C), ("S", "x", -C), ("E", "y", C), ("W", "y", -C)):
    for k, p in enumerate((-C, -C / 2, 0, C / 2)):
        mid = p + 150
        arch_half(f"Arch_{side}_{k}_a", axis, c, (p + 24, 240), (mid, 290), 26, 60, PLASTER, cusps=(0.3, 0.68))
        arch_half(f"Arch_{side}_{k}_b", axis, c, (p + 300 - 24, 240), (mid, 290), 26, 60, PLASTER, cusps=(0.3, 0.68))
    wall_piece(f"Fascia_{side}", axis, c, -C - 22, C + 22, 295, H + T, thick=36, folder="Arches", collide=False)
    chh = c - 25 if c > 0 else c + 25
    wall_piece(f"Chhajja_{side}", axis, chh, -C - 50, C + 50, H + T - 4, H + T + 6, thick=50, mat=DARK, folder="Arches", collide=False)

# Veranda ceiling joists (dark wood). The ones under the broken roof patches are snapped and hanging.
def joist_broken(label, axis, c, a_from, a_to):
    """Beam along the veranda depth: a short stub, then a snapped piece hanging down."""
    sgn = 1 if a_to > a_from else -1
    stub_end = a_from + sgn * 80
    drop, run = 55 * math.sin(math.radians(50)), 55 * math.cos(math.radians(50))
    if axis == "y":   # beam runs along Y at x=c
        piece(f"{label}_stub", (c, (a_from + stub_end) / 2, 339), (14, 80, 22), WOOD, folder="Veranda")
        piece(f"{label}_hang", (c, stub_end + sgn * run, 339 - drop), (110, 14, 20), WOOD, (-50, 90 * sgn, 0), folder="Veranda")
        piece(f"{label}_end", (c, a_to - sgn * 30, 339), (14, 60, 22), WOOD, folder="Veranda")
    else:
        piece(f"{label}_stub", ((a_from + stub_end) / 2, c, 339), (80, 14, 22), WOOD, folder="Veranda")
        piece(f"{label}_hang", (stub_end + sgn * run, c, 339 - drop), (110, 14, 20), WOOD, (-50, 0 if sgn > 0 else 180, 0), folder="Veranda")
        piece(f"{label}_end", (a_to - sgn * 30, c, 339), (60, 14, 22), WOOD, folder="Veranda")


for x in range(-800, 801, 200):
    for side, (ya, yb) in (("N", (C + 18, V - T / 2)), ("S", (-V + T / 2, -C - 18))):
        if side == "S" and HOLE_VERANDA_S[0] - 20 < x < HOLE_VERANDA_S[1] + 20:
            joist_broken(f"Joist_S_{x}", "y", x, -V + T / 2, -C - 18)
            continue
        piece(f"Joist_{side}_{x}", (x, (ya + yb) / 2, 339), (14, yb - ya, 22), WOOD, folder="Veranda")
for y in range(-500, 501, 200):
    for side, (xa, xb) in (("E", (C + 18, V - T / 2)), ("W", (-V + T / 2, -C - 18))):
        if side == "W" and HOLE_VERANDA_W[2] - 20 < y < HOLE_VERANDA_W[3] + 20:
            joist_broken(f"Joist_W_{y}", "x", y, -V + T / 2, -C - 18)
            continue
        piece(f"Joist_{side}_{y}", ((xa + xb) / 2, y, 339), (xb - xa, 14, 22), WOOD, folder="Veranda")

# The well: stone drum, rim, wooden frame, rope and an old brass bucket.
well = box("Well", -90, 90, -90, 90, 0, 80, folder="Courtyard", mesh=cylinder, mat=WELLSTONE)
box("Well_Post_A", -110, -95, -8, 8, 0, 200, folder="Courtyard", mat=WOOD)
box("Well_Post_B", 95, 110, -8, 8, 0, 200, folder="Courtyard", mat=WOOD)
box("Well_Beam", -110, 110, -6, 6, 190, 202, folder="Courtyard", mat=WOOD)
piece("Well_Rim", (0, 0, 84), (196, 196, 8), DARK, mesh=cylinder, folder="Courtyard")
piece("Well_Water", (0, 0, 88.5), (160, 160, 1), IRON, mesh=cylinder, folder="Courtyard")
piece("Well_Pulley", (0, 0, 182), (16, 6, 16), WOOD, mesh=cylinder, rot=(0, 0, 90), folder="Courtyard")
piece("Well_Rope", (0, 0, 135), (1.5, 1.5, 95), CLOTH, folder="Courtyard")
if not prop("Well_Bucket", "wooden_bucket_01", 128, 42, yaw=30, scale=0.9, folder="Courtyard"):
    piece("Well_Bucket", (55, -60, 100), (26, 26, 22), BRASS, mesh=cylinder, rot=(0, 0, 12), folder="Courtyard")

# Furniture (old dark wood).
box("Bed_NE", 900, 1300, 1050, 1330, 0, 55, folder="Furniture", mat=WOOD)
box("Bed_E1", 1100, 1380, 300, 700, 0, 55, folder="Furniture", mat=WOOD)
box("Kitchen_Counter", -1380, -1000, -1380, -1250, 0, 90, folder="Furniture", mat=DARK)
box("Prayer_Altar", -150, 150, 1250, 1380, 0, 60, folder="Furniture", mat=DARK)
box("Dining_Table", -1300, -1000, 250, 650, 0, 75, folder="Furniture", mat=WOOD)
# Old tall clock against the clock room's east wall (front turned to face west).
if not prop("Clock_Tall", "vintage_grandfather_clock_01", 1355, -440, yaw=90, height=205, folder="Furniture"):
    box("Clock_Cabinet", 1300, 1380, -500, -380, 0, 200, folder="Furniture", mat=WOOD)
box("Guest_Cot", 1000, 1350, -1350, -1150, 0, 45, folder="Furniture", mat=WOOD)
piece("Bed_NE_Mattress", (1100, 1190, 59), (390, 270, 8), CLOTH, rot=(0, 1.5, 0.8), folder="Furniture")
piece("Guest_Cot_Sheet", (1175, -1250, 47), (330, 180, 4), CLOTH, rot=(0, -2, 0), folder="Furniture")

# Doors.
for x, name in ((-700, "Store"), (0, "Prayer"), (700, "Bedroom")):
    door_in_x_wall(f"Door_{name}", x, V)
for x, name in ((-700, "Kitchen"), (0, "Entrance"), (700, "Guest")):
    door_in_x_wall(f"Door_{name}", x, -V)
door_in_y_wall("Door_Bedroom2", V, 450)
door_in_y_wall("Door_ClockRoom", V, -450)
door_in_y_wall("Door_Dining", -V, 450)
door_in_y_wall("Door_Store2", -V, -450)
door_in_x_wall("Door_FrontGate", 0, -O - T / 2, locked=True)

# ------------------------------------------------------------------ age and decay

def rubble(label, cx, cy, radius, count, mats=(PLASTER, STONE), max_size=26):
    """A low heap of fallen plaster and brick: randomly rotated small boxes, bigger in the middle."""
    for i in range(count):
        r = radius * math.sqrt(rng.random())
        a = rng.uniform(0, 2 * math.pi)
        s = rng.uniform(6, max_size) * (1.0 - 0.5 * r / radius)
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        size = (s * rng.uniform(0.8, 1.6), s * rng.uniform(0.8, 1.4), s * rng.uniform(0.3, 0.8))
        piece(f"{label}_{i}", (x, y, size[2] * 0.3), size, rng.choice(mats),
              (rng.uniform(-25, 25), rng.uniform(0, 360), rng.uniform(-25, 25)), folder="Decay")


def chunks(label, cx, cy, spread, count):
    """Flat flakes of plaster fallen off a wall."""
    for i in range(count):
        x, y = cx + rng.uniform(-spread, spread), cy + rng.uniform(-spread, spread)
        piece(f"{label}_{i}", (x, y, 1.5), (rng.uniform(12, 30), rng.uniform(10, 22), 3), PLASTER,
              (rng.uniform(-6, 6), rng.uniform(0, 360), rng.uniform(-6, 6)), folder="Decay")


def bare_brick(label, axis, face, a, z, w, h, inward):
    """A patch where the plaster fell off and the brick shows, with flakes on the floor below."""
    if axis == "x":
        piece(label, (a, face + inward * 1.0, z), (w, 2, h), CLAY, (0, 0, rng.uniform(-8, 8)), folder="Decay")
        chunks(f"{label}_flakes", a, face + inward * 30, 35, 3)
    else:
        piece(label, (face + inward * 1.0, a, z), (2, w, h), CLAY, (rng.uniform(-8, 8), 0, 0), folder="Decay")
        chunks(f"{label}_flakes", face + inward * 30, a, 35, 3)


IN = V - T / 2      # inner wall face on the veranda side (887.5)
for i, (axis, face, a, z, w, h, inward) in enumerate((
        ("x", IN, -350, 150, 70, 45, -1), ("x", IN, 430, 270, 55, 35, -1),          # veranda, north wall
        ("x", -IN, -450, 90, 60, 40, 1), ("x", -IN, 340, 240, 80, 50, 1),          # veranda, south wall
        ("y", IN, -220, 200, 60, 45, -1), ("y", -IN, 110, 130, 75, 40, 1),         # veranda, east/west walls
        ("x", O, 1250, 280, 70, 40, -1), ("x", -O, -560, 210, 60, 45, 1))):         # bedroom, kitchen
    bare_brick(f"Brick_{i}", axis, face, a, z, w, h, inward)

# Rubble heaps in room corners and under the broken roof (never in doorways or almirah fronts).
rubble("Rubble_Store", -1000, 1330, 55, 10)
rubble("Rubble_Store2", -1050, -70, 45, 8)
rubble("Rubble_Dining", -960, 840, 40, 7)
rubble("Rubble_Kitchen", -1360, -1160, 30, 5)
rubble("Rubble_VerandaNE", 850, 845, 40, 7)
rubble("Rubble_RoofHoleS", (HOLE_VERANDA_S[0] + HOLE_VERANDA_S[1]) / 2, -835, 60, 12, mats=(PLASTER, STONE, WOOD), max_size=34)
rubble("Rubble_RoofHoleW", -835, (HOLE_VERANDA_W[2] + HOLE_VERANDA_W[3]) / 2, 55, 10, mats=(PLASTER, STONE, WOOD), max_size=34)
rubble("Rubble_Clock", 1330, -830, 45, 8, mats=(PLASTER, STONE, WOOD))
# Jagged broken edges hanging round the roof holes.
for hname, (hx0, hx1, hy0, hy1) in (("S", HOLE_VERANDA_S), ("W", HOLE_VERANDA_W), ("C", HOLE_CLOCK)):
    for j in range(4):
        x, y = rng.uniform(hx0, hx1), rng.choice((hy0, hy1))
        if j % 2:
            x, y = rng.choice((hx0, hx1)), rng.uniform(hy0, hy1)
        piece(f"RoofBreak_{hname}_{j}", (x, y, H - 6), (rng.uniform(25, 45), rng.uniform(15, 30), 12), PLASTER,
              (rng.uniform(-20, 20), rng.uniform(0, 360), rng.uniform(-20, 20)), folder="Decay")

# A roof beam that came down through the clock room ceiling and leans against the south wall.
piece("FallenBeam_Clock", (1180, -830, 165), (470, 22, 24), WOOD, (38, 0, 6), folder="Decay")
piece("FallenBeam_Veranda", (-215, -760, 12), (200, 16, 18), WOOD, (4, 70, 0), folder="Decay")

# A broken charpai (rope cot) in the store room: far legs snapped, frame slumped to the floor.
def broken_cot(label, cx, cy, yaw):
    ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def at(lx, ly, z):
        return (cx + lx * ca - ly * sa, cy + lx * sa + ly * ca, z)

    tilt = math.degrees(math.atan2(36, 190))
    for side in (-42, 42):
        piece(f"{label}_rail{side}", at(0, side, 24), (196, 7, 7), WOOD, (tilt, yaw, 0), folder="Decay")
    piece(f"{label}_railNear", at(-95, 0, 42), (7, 90, 7), WOOD, (0, yaw, 0), folder="Decay")
    piece(f"{label}_railFar", at(95, 0, 6), (7, 90, 7), WOOD, (0, yaw, 8), folder="Decay")
    for side in (-42, 42):
        piece(f"{label}_leg{side}", at(-95, side, 20), (8, 8, 40), WOOD, (0, yaw, 0), folder="Decay")
    piece(f"{label}_stump", at(95, -42, 3), (8, 8, 6), WOOD, (0, yaw, 0), folder="Decay")
    piece(f"{label}_fallenLeg", at(120, 55, 4), (38, 8, 8), WOOD, (0, yaw + 35, 0), folder="Decay")
    # Torn rope weave: two sagging pieces with a gap.
    piece(f"{label}_weaveA", at(-45, 0, 30), (90, 78, 2), CLOTH, (tilt + 2, yaw, 0), folder="Decay")
    piece(f"{label}_weaveB", at(62, 8, 9), (55, 60, 2), CLOTH, (tilt + 6, yaw, 4), folder="Decay")


broken_cot("BrokenCot", -600, 1230, 95)

# Clay pots (matkas): squat body, neck, rim.
def pot(label, x, y, d, z=0.0, tipped=False, collide=False):
    if tipped:
        piece(f"{label}", (x, y, z + d * 0.42), (d * 1.05, d, d * 0.85), CLAY, (0, rng.uniform(0, 360), 75), mesh=sphere, folder="Pots")
        return
    piece(f"{label}", (x, y, z + d * 0.42), (d, d, d * 0.85), CLAY, mesh=sphere, folder="Pots", collide=collide)
    piece(f"{label}_neck", (x, y, z + d * 0.88), (d * 0.42, d * 0.42, d * 0.16), CLAY, mesh=cylinder, folder="Pots")
    piece(f"{label}_rim", (x, y, z + d * 0.97), (d * 0.52, d * 0.52, d * 0.05), CLAY, mesh=cylinder, folder="Pots")


def shards(label, x, y, count):
    for i in range(count):
        piece(f"{label}_{i}", (x + rng.uniform(-30, 30), y + rng.uniform(-30, 30), 1.5), (rng.uniform(6, 14), rng.uniform(5, 10), 2),
              CLAY, (rng.uniform(-15, 15), rng.uniform(0, 360), rng.uniform(-15, 15)), folder="Pots")


def bpot(label, x, y, d, z=0.0, model="planter_pot_clay", yaw=None, kind=None):
    """Breakable clay pot / jar (Poly Haven model), d = rough height in cm. Falls back to the sphere pot."""
    yaw = rng.uniform(0, 360) if yaw is None else yaw
    a = breakable(label, model, x, y, z, yaw, height=d, kind=kind or POT, shards=10 + int(d / 6))
    if a is None:
        pot(label, x, y, d, z=z, collide=True)
    return a


# Kitchen: water jars along the south wall, small ones on the counter.
bpot("Pot_Kitchen_1", -560, -1340, 42, model="ceramic_pot")
bpot("Pot_Kitchen_2", -630, -1362, 30)
bpot("Pot_Kitchen_3", -880, -1352, 40)
bpot("Pot_Kitchen_4", -1335, -1300, 30, z=90, model="ceramic_vase_04")
bpot("Pot_Kitchen_5", -1060, -1318, 20, z=90)
bpot("Pot_Kitchen_7", -800, -1365, 26, model="ceramic_vase_04")
pot("Pot_Kitchen_6", -520, -1260, 30, tipped=True)
shards("Shards_Kitchen", -560, -1230, 4)
# Courtyard corners.
bpot("Pot_Court_1", -525, -515, 50)
bpot("Pot_Court_2", -468, -548, 32, model="ceramic_vase_04")
shards("Shards_Court", -440, -480, 5)
bpot("Pot_Court_3", 530, 500, 46)
bpot("Pot_Court_4", 470, 552, 30, model="ceramic_pot")
bpot("Pot_Court_6", 535, -525, 40)
pot("Pot_Court_5", -520, 500, 40, tipped=True)
shards("Shards_Court2", -470, 470, 4)
# Veranda corners and the dining room.
bpot("Pot_Veranda_SW", -852, -822, 44)
bpot("Pot_Veranda_SW2", -820, -862, 30, model="ceramic_vase_04")
bpot("Pot_Veranda_NW", -848, 835, 38, model="ceramic_pot")
bpot("Pot_Dining", -1345, 95, 36, model="ceramic_pot")


def crate(label, model, x, y, z=0.0, yaw=0.0, scale=1.0):
    return breakable(label, model, x, y, z, yaw, scale=scale, kind=CRATE, shards=14)


# Store room: crates stacked against the north and west walls (the battery is in front of them).
if crate("Crate_Store_1", "wooden_crate_02", -1250, 1343, yaw=90) is None:
    box("Store_Crates_1", -1350, -1150, 1150, 1350, 0, 120, folder="Furniture", mat=WOOD)
crate("Crate_Store_2", "wooden_crate_01", -1245, 1345, z=45, yaw=4)
crate("Crate_Store_3", "wooden_crate_01", -1345, 1180, yaw=-90)
# Store room 2: a stack in the south-west corner, one more by the window.
if crate("Crate_Store2_1", "wooden_crate_02", -1352, -640, yaw=0) is None:
    box("Store_Crates_2", -1350, -1200, -700, -500, 0, 150, folder="Furniture", mat=WOOD)
crate("Crate_Store2_2", "wooden_crate_01", -1350, -640, z=45, yaw=-88)
crate("Crate_Store2_3", "wooden_crate_01", -1240, -835, yaw=12)
crate("Crate_Veranda_SE", "wooden_crate_01", 835, -848, yaw=-35, scale=0.9)

# Decor props (static): trunks, a charkha, stools, a basket, a chest of drawers, brass pots.
prop("Chest_Store2", "treasure_chest", -1110, -852, yaw=0, scale=0.9, folder="Furniture")
prop("Chest_Bedroom", "treasure_chest", 1110, 1005, yaw=180, scale=0.9, folder="Furniture")
prop("Charkha_Store", "spinning_wheel_01", -780, 1335, yaw=90, folder="Furniture")
prop("Drawers_Guest", "vintage_wooden_drawer_01", 525, -1160, yaw=-90, scale=0.9, folder="Furniture")
prop("SideTable_Bedroom2", "WoodenTable_02", 1350, 820, height=55, folder="Furniture")
prop("Stool_Dining", "folding_wooden_stool", -1050, 705, yaw=15, folder="Furniture")
prop("Patla_Kitchen", "wooden_stool_02", -800, -1245, yaw=20, scale=1.3, folder="Furniture")
prop("Basket_Kitchen", "wicker_basket_01", -960, -1215, yaw=-8, scale=1.2, folder="Furniture", collide=False)
prop("BrassPot_Kitchen", "brass_pot_01", -950, -1345, scale=0.9, folder="Furniture")
for side in (-1, 1):
    prop(f"Kalash_Altar_{side}", "brass_vase_03", side * 128, 1310, z=60, scale=1.1, folder="Prayer", collide=False)

# Tulsi planter in the courtyard, the plant long dead.
piece("Tulsi_Base", (-420, 400, 35), (64, 64, 70), PLASTER, folder="Courtyard", collide=True)
piece("Tulsi_Top", (-420, 400, 74), (76, 76, 8), DARK, folder="Courtyard")
for j in range(4):
    piece(f"Tulsi_Twig_{j}", (-420 + rng.uniform(-10, 10), 400 + rng.uniform(-10, 10), 98), (2, 2, rng.uniform(30, 50)), WOOD,
          (rng.uniform(-30, 30), rng.uniform(0, 360), rng.uniform(-30, 30)), folder="Courtyard")

# Dry leaves blown into the courtyard corners and along the pillars, a few on the veranda.
for i in range(46):
    if i < 30:
        cx, cy = rng.choice(((-520, -520), (520, 520), (-520, 520), (520, -520), (-560, 0), (560, 150), (0, 560)))
        x, y = cx + rng.gauss(0, 60), cy + rng.gauss(0, 60)
    elif i < 40:
        x, y = rng.uniform(-560, 560), rng.uniform(-560, 560)
        if math.hypot(x, y) < 160:
            continue
    else:
        x, y = rng.choice(((rng.uniform(-850, 850), rng.uniform(620, 880)), (rng.uniform(-850, 850), rng.uniform(-880, -620))))
    piece(f"Leaf_{i}", (x, y, 0.6), (rng.uniform(7, 13), rng.uniform(4, 7), 0.8), LEAF,
          (rng.uniform(-8, 8), rng.uniform(0, 360), rng.uniform(-8, 8)), folder="Leaves")

# Prayer room: a carved stone niche with a pointed arch behind the altar, brass diya stand under the lamp.
for side in (-1, 1):
    piece(f"Shrine_Pilaster_{side}", (side * 70, O - 5, 140), (16, 10, 160), DARK, folder="Prayer")
piece("Shrine_Lintel", (0, O - 5, 236), (176, 10, 32), DARK, folder="Prayer")
arch_half("Shrine_Arch_L", "x", O - 4, (-62, 172), (0, 216), 7, 18, DARK, cusps=(0.35, 0.7), cusp_size=9, folder="Prayer")
arch_half("Shrine_Arch_R", "x", O - 4, (62, 172), (0, 216), 7, 18, DARK, cusps=(0.35, 0.7), cusp_size=9, folder="Prayer")
piece("Shrine_Back", (0, O - 1, 160), (130, 2, 190), DARK, folder="Prayer")   # dark recess (clay glowed too hot by the diya)
piece("Diya_Stand_Base", (0, 1300, 61.5), (20, 20, 3), BRASS, mesh=cylinder, folder="Prayer")
piece("Diya_Stand_Stem", (0, 1300, 71.5), (4, 4, 17), BRASS, mesh=cylinder, folder="Prayer")
piece("Diya_Stand_Dish", (0, 1300, 81.5), (14, 14, 3), BRASS, mesh=cylinder, folder="Prayer")
for j, (dx, dy) in enumerate(((-100, 1290), (95, 1300), (-60, 1330), (60, 1335))):
    piece(f"Clay_Diya_{j}", (dx, dy, 62), (9, 9, 4), CLAY, mesh=sphere, folder="Prayer")

# Old frames behind the mirrors.
piece("MirrorFrame_Bedroom", (700, O - 1, 150), (84, 2, 114), BRASS, folder="Mirrors")
piece("MirrorFrame_Bedroom2", (O - 1, 600, 150), (2, 84, 114), BRASS, folder="Mirrors")
piece("MirrorFrame_Guest", (1000, -O + 1, 150), (84, 2, 114), BRASS, folder="Mirrors")


def lantern(label, x, y, z, hang=True):
    """Small brass lantern frame around a light at (x, y, z), hanging on a chain from the ceiling."""
    piece(f"{label}_cap", (x, y, z + 17), (22, 22, 5), BRASS, mesh=cylinder, folder="Lanterns")
    piece(f"{label}_finial", (x, y, z + 24), (6, 6, 10), BRASS, mesh=cylinder, folder="Lanterns")
    piece(f"{label}_base", (x, y, z - 16), (20, 20, 4), BRASS, mesh=cylinder, folder="Lanterns")
    for j, (dx, dy) in enumerate(((-8, -8), (8, 8))):
        piece(f"{label}_post{j}", (x + dx, y + dy, z), (1.5, 1.5, 30), BRASS, folder="Lanterns")
    if hang:
        piece(f"{label}_chain", (x, y, (z + 29 + H) / 2), (1.2, 1.2, H - z - 29), IRON, folder="Lanterns")


# Breakable lanterns (R21): each carries its own warm light, which goes out when the Gun Torch smashes it.
# (label, model, x, y, z, yaw, height, flame height above the base, candela, radius, glass slot)
LANTERNS = (
    ("Lantern_Veranda", "brass_diya_lantern", -665, 665, 172, 0, 62, 22, LAMP_CANDELA * 1.3, 800.0, 1),   # hangs off the NW pillar
    ("Lamp_Kitchen", "Lantern_01", -1220, -1310, 90, 20, 30, 10, LAMP_CANDELA, 650.0, 1),               # on the counter
    ("Lantern_Dining", "Lantern_01", -1150, 470, 75, -30, 30, 10, LAMP_CANDELA * 0.6, 450.0, 1),       # on the dining table
    ("Lantern_Bedroom2", "wooden_lantern_01", 1350, 820, 55, 200, 34, 13, LAMP_CANDELA * 0.5, 420.0, 1),  # on the side table
    ("Lantern_Entrance", "wooden_lantern_01", -420, -1350, 0, 10, 38, 14, LAMP_CANDELA * 0.45, 420.0, 1),  # left on the floor
)
breakable_lamps = set()
for label, model, lx, ly, lz, lyaw, lh, flame_z, cd, rad, glass in LANTERNS:
    if breakable(label, model, lx, ly, lz, lyaw, height=lh, kind=LANTERN, shards=10,
                 light=(flame_z, cd, rad), glow_slot=glass, folder="Lanterns"):
        breakable_lamps.add(label)
if "Lantern_Veranda" in breakable_lamps:
    piece("Lantern_Veranda_Chain", (-665, 665, (172 + 62 + H) / 2), (1.2, 1.2, H - 172 - 62), IRON, folder="Lanterns")
else:
    lantern("Lantern_Veranda_Frame", -665, 665, 210)
if "Lamp_Kitchen" not in breakable_lamps:
    piece("Lamp_Kitchen_Base", (-1200, -1300, 92), (16, 16, 4), BRASS, mesh=cylinder, folder="Lanterns")
    piece("Lamp_Kitchen_Stem", (-1200, -1300, 100), (4, 4, 12), BRASS, mesh=cylinder, folder="Lanterns")
    piece("Lamp_Kitchen_Dish", (-1200, -1300, 107), (12, 12, 3), BRASS, mesh=cylinder, folder="Lanterns")


# ------------------------------------------------------------------ set dressing (R17): flames, cobwebs, cloth, stains, dust

decal_count = 0


def flame(label, x, y, z, s=1.0):
    """A small emissive flame (blooms) on a lamp / diya."""
    piece(label, (x, y, z + 2.2 * s), (1.6 * s, 1.6 * s, 4.2 * s), FLAME, mesh=sphere, folder="Lanterns").static_mesh_component.set_cast_shadow(False)
    piece(f"{label}_core", (x, y, z + 1.2 * s), (2.2 * s, 2.2 * s, 2.0 * s), FLAME, mesh=sphere, folder="Lanterns").static_mesh_component.set_cast_shadow(False)


flame("Flame_Diya_Prayer", 0, 1300, 83)
if "Lamp_Kitchen" not in breakable_lamps:
    flame("Flame_Lamp_Kitchen", -1200, -1300, 108.5)
if "Lantern_Veranda" not in breakable_lamps:
    flame("Flame_Lantern_Veranda", -665, 665, 196, 1.6)
for j, (dx, dy) in enumerate(((-100, 1290), (95, 1300), (-60, 1330), (60, 1335))):
    flame(f"Flame_Clay_Diya_{j}", dx, dy, 64, 0.8)


def cobweb(label, x, y, z, yaw, size=90.0, tilt=-45.0):
    """Web across a corner: plane centred on the diagonal, top edge tucked into the corner."""
    a = piece(label, (x, y, z), (size, size, 1), COBWEB, (tilt, yaw, 0.0), mesh=plane, folder="Cobwebs")
    a.static_mesh_component.set_cast_shadow(False)
    return a


def corner_web(label, cx, cy, sx, sy, size=90.0, z=H):
    """Ceiling corner at (cx, cy); (sx, sy) = +-1 pointing into the room."""
    d = size * 0.33
    yaw = math.degrees(math.atan2(sy, sx))
    cobweb(label, cx + sx * d * 0.707, cy + sy * d * 0.707, z - d, yaw, size)


IN_O = O - T / 2 + 0.0     # room side of the outer walls
for i, (cx, cy, sx, sy, sz) in enumerate((
        (-IN_O, IN_O, 1, -1, 100), (IN_O, IN_O, -1, -1, 110), (-470 + T / 2, IN_O, 1, -1, 80),     # north rooms
        (IN_O, V + T / 2, -1, 1, 80), (-IN_O, -IN_O, 1, 1, 100), (IN_O, -IN_O, -1, 1, 90),           # bedroom, kitchen, guest
        (470 - T / 2, -IN_O, -1, 1, 70), (-470 + T / 2, -V - T / 2, 1, -1, 75),                     # entrance hall
        (IN_O, -T / 2, -1, -1, 80), (-IN_O, T / 2, 1, 1, 85), (-IN_O, -V - T / 2 + 0, 1, -1, 0),    # clock, dining
        (V - T / 2, V - T / 2, -1, -1, 95), (-V + T / 2, -V + T / 2, 1, 1, 95),                     # veranda corners
        (V - T / 2, -V + T / 2, -1, 1, 70), (-V + T / 2, V - T / 2, 1, -1, 80))):
    if sz:
        corner_web(f"Cobweb_{i}", cx, cy, sx, sy, sz)
# Webs strung between the veranda joists and under the arcade brackets.
for i, (cx, cy, sx, sy) in enumerate(((-593, IN, 1, -1), (207, IN, -1, -1), (393, -IN, 1, 1), (IN, 107, -1, -1), (-IN, -307, 1, 1))):
    corner_web(f"Cobweb_Joist_{i}", cx, cy, sx, sy, 60, z=328)
for i, (x, y, yaw) in enumerate(((-300, C, 90), (C, 300, 0), (-C, -300, 0), (300, -C, 90))):
    cobweb(f"Cobweb_Bracket_{i}", x, y, 268, yaw, 45, tilt=-70)


def torn_curtain(label, axis, c, a0, a1, top=294.0, strips=6):
    """Tattered strips of old muslin hanging from an arch / window head (no collision)."""
    for j in range(strips):
        w = rng.uniform(16, 34)
        length = rng.uniform(90, 215)
        a = rng.uniform(a0 + w / 2, a1 - w / 2)
        off = rng.uniform(-6, 6)
        sway = rng.uniform(-6, 6)
        if axis == "x":
            centre, yaw = (a, c + off, top - length / 2), rng.uniform(-8, 8)
        else:
            centre, yaw = (c + off, a, top - length / 2), 90 + rng.uniform(-8, 8)
        piece(f"{label}_{j}", centre, (w, length, 1), MUSLIN, (sway, yaw, 90.0), mesh=plane, folder="Cloth")


torn_curtain("Curtain_ArchN1", "x", C, -C / 2 - 270, -C / 2 - 30)
torn_curtain("Curtain_ArchW2", "y", -C, 30, 270, strips=5)
torn_curtain("Curtain_ArchE0", "y", C, -570, -330, strips=4)
torn_curtain("Curtain_WinStore2", "y", -O + T / 2 + 4, -80 - 45, -80 + 45, top=WIN_Z1 + 8, strips=3)
torn_curtain("Curtain_WinGuest", "x", -O + T / 2 + 4, 1250 - 45, 1250 + 45, top=WIN_Z1 + 8, strips=3)


def decal(label, mat, loc, rot, size):
    """size = (depth, width, height) in cm, full extents."""
    global decal_count
    if mat is None:
        return None
    decal_count += 1
    a = actor_sub.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]))
    a.set_actor_label(label)
    a.set_folder_path("Decals")
    dc = a.get_editor_property("decal")
    dc.set_decal_material(mat)
    dc.set_editor_property("decal_size", unreal.Vector(size[0] / 2, size[1] / 2, size[2] / 2))
    return a


def wall_decal(label, mat, axis, face, inward, a, z, w, h):
    """Patch on a wall face (axis 'x' = wall along X at y=face). inward = +-1 towards the room."""
    if axis == "x":
        return decal(label, mat, (a, face, z), (0, 90 if inward > 0 else -90, rng.uniform(-6, 6)), (24, w, h))
    return decal(label, mat, (face, a, z), (0, 0 if inward > 0 else 180, rng.uniform(-6, 6)), (24, w, h))


# Water stains running down from the roof line / broken roof, soot above the lamps.
for i, (axis, face, inward, a, z, w, h) in enumerate((
        ("x", -IN, 1, -230, 250, 190, 200), ("y", -IN, 1, 215, 250, 170, 210),        # under the veranda roof holes
        ("x", IN, -1, 300, 270, 160, 150), ("x", IN, -1, -520, 240, 120, 210),        # veranda north wall
        ("y", IN, -1, 200, 260, 150, 170), ("x", -IN, 1, 520, 230, 110, 230),
        ("x", O - T / 2, -1, -900, 250, 200, 200), ("x", O - T / 2, -1, 1000, 260, 140, 170),   # store / bedroom
        ("y", O - T / 2, -1, -700, 240, 220, 220), ("y", O - T / 2, -1, 300, 250, 160, 180),    # clock room / bedroom 2
        ("y", -O + T / 2, 1, 650, 250, 180, 200), ("x", -O + T / 2, 1, -1000, 250, 150, 190),   # dining, kitchen
        ("x", -O + T / 2, 1, 300, 240, 140, 200), ("x", O - T / 2, -1, -250, 220, 120, 200),    # entrance, prayer
        ("y", -O + T / 2, 1, -600, 230, 120, 230), ("x", -O + T / 2, 1, 900, 260, 170, 160))):  # store 2, guest
    wall_decal(f"Stain_{i}", DECAL_STAIN, axis, face, inward, a, z, w, h)
for i, (axis, face, inward, a, z, w, h) in enumerate((
        ("x", -O + T / 2, 1, -1200, 220, 110, 170),     # behind the kitchen lamp
        ("x", O - T / 2, -1, 0, 290, 160, 110),         # above the prayer niche
        ("x", -O + T / 2, 1, -900, 300, 220, 90),       # kitchen ceiling line (old cooking fire)
        ("y", -IN, 1, 665, 300, 140, 90))):             # above the veranda lantern
    wall_decal(f"Soot_{i}", DECAL_SOOT, axis, face, inward, a, z, w, h)
# A dark scorch round the prayer-room diya on the ceiling.
decal("Soot_PrayerCeiling", DECAL_SOOT, (0, 1300, H), (-90, 0, 0), (30, 120, 120))


def floor_dust(label, axis, face, a0, a1, width=80.0):
    """Grey dust banked against a wall: decal centred on the wall line, fading towards the room."""
    if axis == "x":
        return decal(label, DECAL_DUST, ((a0 + a1) / 2, face, 0), (-90, 90, 0), (30, a1 - a0, width))
    return decal(label, DECAL_DUST, (face, (a0 + a1) / 2, 0), (-90, 0, 0), (30, a1 - a0, width))


for i, (axis, face, a0, a1) in enumerate((
        ("x", O - T / 2, -1350, -800), ("x", O - T / 2, 800, 1250), ("x", O - T / 2, -380, 380),     # north rooms (back wall)
        ("x", -O + T / 2, -1350, -800), ("x", -O + T / 2, 800, 1350), ("x", -O + T / 2, -420, -100), # south rooms
        ("y", -O + T / 2, 950, 1350), ("y", -O + T / 2, 100, 600), ("y", O - T / 2, 100, 500),     # west / east rooms
        ("y", O - T / 2, -850, -550), ("y", -O + T / 2, -850, -450),
        ("x", IN, -550, -100), ("x", IN, 100, 550), ("x", -IN, 100, 550), ("x", -IN, -550, -100),   # veranda inner walls
        ("y", IN, -350, 350), ("y", -IN, -350, 350))):
    floor_dust(f"Dust_{i}", axis, face, a0, a1, 90 if axis == "x" and abs(face) == IN else 110)
# Damp ring and dust around the well's base.
decal("Dust_Well", DECAL_DUST, (0, 0, 0), (-90, 0, 0), (30, 280, 280))

# Night sky: an unlit, unfogged dome (gradient, clouds, stars, the moon). No shadows, no GI.
sky = piece("NightSky", (0, 0, 0), (16000, 16000, 16000), SKY, mesh=sphere, folder="Lighting")
sky_c = sky.static_mesh_component
sky_c.set_cast_shadow(False)
for prop in ("affect_distance_field_lighting", "affect_dynamic_indirect_lighting", "receives_decals"):
    try:
        sky_c.set_editor_property(prop, False)
    except Exception as exc:
        unreal.log_warning(f"Sky: {prop}: {exc}")

# ------------------------------------------------------------------ clues & ritual

mirror_cls = cls("MirrorActor")
clue_cls = cls("ClueActor")


def mirror(label, loc, yaw, sign_id="", name="", note=""):
    a = spawn(mirror_cls, loc, yaw, label, "Mirrors")
    if sign_id:
        comp = a.get_component_by_class(unreal.ClueComponent)
        comp.set_editor_property("sign_id", sign_id)
        comp.set_editor_property("display_name", name)
        comp.set_editor_property("description", note)
        comp.set_editor_property("clue_type", unreal.ClueType.VISUAL)
    return a


def clue(label, loc, scale, sign_id, name, note, clue_type, red_herring=False, pick_up=False, requires_uv=False):
    a = spawn(clue_cls, loc, 0, label, "Clues")
    a.set_actor_scale3d(unreal.Vector(*scale))
    a.set_editor_property("pick_up", pick_up)
    if requires_uv:
        a.set_editor_property("requires_uv", True)
    comp = a.get_component_by_class(unreal.ClueComponent)
    comp.set_editor_property("sign_id", sign_id)
    comp.set_editor_property("display_name", name)
    comp.set_editor_property("description", note)
    comp.set_editor_property("clue_type", clue_type)
    comp.set_editor_property("is_red_herring", red_herring)
    return a


# Mirror glass faces local +X. Three mirrors: cover two, trap her in the third.
mirror("Mirror_Bedroom", (700, O - 3, 150), -90, "NoReflection", "Bedroom mirror", "My torch shows in it. I don't.")
mirror("Mirror_Bedroom2", (O - 3, 600, 150), 180)
mirror("Mirror_Guest", (1000, -O + 3, 150), 90)

clue("Clue_Hair", (650, 1000, 1), (1.2, 1.2, 0.1), "CombedHair", "Combed hair",
     "Long black hair, neatly combed. On the floor by the mirror.", unreal.ClueType.VISUAL)
clue("Clue_Diary", (1200, 150, 56), (1.0, 1.4, 0.1), "Humming", "Diary page",
     "'Every night she hums the same song. Ma says not to answer.'", unreal.ClueType.WRITTEN, pick_up=True)
# UV-only: small handprints on the bedroom wall, reaching for the mirror (scale x 100 cm = size: 60 x 2 x 45 cm).
clue("Clue_UV_Handprints", (585, O - 1, 115), (0.6, 0.02, 0.45), "NoReflection", "Handprints on the wall",
     "Small hands pressed against the wall, reaching for the mirror. Only visible under UV.",
     unreal.ClueType.WRITTEN, requires_uv=True)
# Red herrings for the Drowned Child, right by the well.
clue("Clue_WetFootprints", (150, -250, 1), (4.0, 1.0, 0.05), "WetFootprints", "Wet footprints",
     "Small and wet, leading from the well.", unreal.ClueType.VISUAL, red_herring=True)

salt = spawn(cls("RitualItemPickup"), (-1150, -1300, 98), 0, "Item_Salt", "Ritual")
salt.set_editor_property("item_id", "Salt")
salt.set_editor_property("display_name", "a bag of salt")

spot = spawn(cls("RitualSpot"), (0, 0, 1), 0, "RitualSpot_WellSaltCircle", "Ritual")
spot.set_actor_scale3d(unreal.Vector(2.2, 2.2, 1.0))
spot.set_editor_property("ritual_id", "SaltCircle")
spot.set_editor_property("action_text", "Pour a salt circle around the well")
spot.set_editor_property("required_items", ["Salt"])
spot.set_editor_property("required_items_text", "salt")

for i, loc in enumerate(((-1300, 1300, 5), (750, -1100, 5), (-750, 0, 5))):
    b = spawn(cls("BatteryPickup"), loc, 0, f"Battery_{i + 1}", "Pickups")

# ------------------------------------------------------------------ hiding spots (almirahs)

hide_cls = cls("HidingSpot")
# (label, location, yaw): opening (+X) faces into the room. Not in the prayer room.
for label, loc, yaw in (
    ("Almirah_Bedroom", (1340, 950, 0), 180),
    ("Almirah_Kitchen", (-1340, -1000, 0), 0),
    ("Almirah_Guest", (1340, -1000, 0), 180),
    ("Almirah_Dining", (-1340, 800, 0), 0),
    ("Almirah_Store2", (-1340, -250, 0), 0),
    ("Almirah_ClockRoom", (1340, -150, 0), 180),
):
    h = spawn(hide_cls, loc, yaw, label, "Hiding")
    try:
        h.get_editor_property("body").set_material(0, WOOD)
    except Exception as exc:
        unreal.log_warning(f"Almirah material: {exc}")

# ------------------------------------------------------------------ room names (map / HUD)

room_cls = cls("RoomArea")
for name, (x0, x1, y0, y1) in (
    ("Courtyard", (-600, 600, -600, 600)),
    ("Veranda", (-900, 900, 600, 900)),
    ("Veranda", (-900, 900, -900, -600)),
    ("Veranda", (600, 900, -600, 600)),
    ("Veranda", (-900, -600, -600, 600)),
    ("Store room", (-1400, -470, 900, 1400)),
    ("Prayer room", (-470, 470, 900, 1400)),
    ("Bedroom", (470, 1400, 900, 1400)),
    ("Kitchen", (-1400, -470, -1400, -900)),
    ("Entrance hall", (-470, 470, -1400, -900)),
    ("Guest room", (470, 1400, -1400, -900)),
    ("Bedroom 2", (900, 1400, 0, 900)),
    ("Clock room", (900, 1400, -900, 0)),
    ("Dining room", (-1400, -900, 0, 900)),
    ("Store room 2", (-1400, -900, -900, 0)),
):
    ra = spawn(room_cls, ((x0 + x1) / 2, (y0 + y1) / 2, 180), 0, f"Room_{name.replace(' ', '')}_{int((x0 + x1) / 2)}_{int((y0 + y1) / 2)}", "Rooms")
    ra.set_editor_property("room_name", name)
    ra.set_editor_property("show_on_map", True)
    ra.get_editor_property("area").set_box_extent(unreal.Vector((x1 - x0) / 2, (y1 - y0) / 2, 180), True)

# ------------------------------------------------------------------ ghost

points = []
for i, loc in enumerate(((-750, 750, 0), (750, 750, 0), (750, -750, 0), (-750, -750, 0),
                         (0, 1150, 0), (1150, -450, 0), (-1150, 450, 0), (0, 0, 0))):
    tp = spawn(unreal.TargetPoint, loc, 0, f"GhostPoint_{i + 1}", "Ghost")
    points.append(tp)

# She starts in the Bedroom: her "home" for thermometer / EMF readings (just off the bed's edge).
ghost = spawn(cls("GhostBase"), (860, 1150, 0), 0, "Ghost", "Ghost")
ghost.set_editor_property("appear_points", points)

setup = spawn(cls("NightSetup"), (0, -1150, 250), 0, "NightSetup", "Night")
setup.set_editor_property("tonight_ghost", unreal.load_asset("/Game/Ghosts/Ghost_01_MirrorWoman"))
setup.set_editor_property("restless_after_seconds", 150.0)
setup.set_editor_property("first_hunt_after_seconds", 90.0)

spawn(unreal.PlayerStart, (0, -1150, 100), 90, "PlayerStart", "Night")

# Nav mesh for the ghost's hunts (built at runtime too, see DefaultEngine.ini).
nav = spawn(unreal.NavMeshBoundsVolume, (0, -200, 150), 0, "NavBounds", "Night")
nav.set_actor_scale3d(unreal.Vector((2 * O + 200) / 200, (2 * O + 600) / 200, 400 / 200))

# ------------------------------------------------------------------ lighting


def setp(obj, prop, value, what="Lighting"):
    try:
        obj.set_editor_property(prop, value)
    except Exception as exc:
        unreal.log_warning(f"{what}: could not set {prop}: {exc}")


# Moon: steep from the south-west, so it lands in the courtyard and drops visible shafts through
# the broken veranda roof (volumetric fog + volumetric shadow).
moon = spawn(unreal.DirectionalLight, (0, 0, 2000), 0, "Moonlight", "Lighting")
moon.set_actor_rotation(unreal.Rotator(0, MOON_PITCH, MOON_YAW), False)
moon_comp = moon.get_component_by_class(unreal.DirectionalLightComponent)
setp(moon_comp, "intensity", MOON_LUX)
setp(moon_comp, "light_color", unreal.Color(r=160, g=182, b=240, a=255))
setp(moon_comp, "volumetric_scattering_intensity", 18.0)
setp(moon_comp, "cast_volumetric_shadow", True)
setp(moon_comp, "light_source_angle", 0.6)
setp(moon_comp, "specular_scale", 0.6)

# Warm lamps (flicker-free; small pools of light). The veranda lantern hangs just off the NW corner pillar.
for loc, name, cd, rad in (((0, 1300, 90), "Diya_Prayer", LAMP_CANDELA, 700.0),
                           ((-1200, -1300, 115), "Lamp_Kitchen", LAMP_CANDELA, 650.0),
                           ((-665, 665, 205), "Lantern_Veranda", LAMP_CANDELA * 1.3, 800.0),
                           ((0, 1340, 64), "Diyas_Altar", LAMP_CANDELA * 0.3, 300.0)):
    if name in breakable_lamps:
        continue   # that lamp is a BreakableProp with its own light (see LANTERNS)
    lamp = spawn(unreal.PointLight, loc, 0, name, "Lighting")
    lc = lamp.get_component_by_class(unreal.PointLightComponent)
    setp(lc, "intensity_units", unreal.LightUnits.CANDELAS)
    setp(lc, "intensity", cd)
    setp(lc, "light_color", unreal.Color(r=255, g=140, b=48, a=255))
    setp(lc, "attenuation_radius", rad)
    setp(lc, "source_radius", 2.0)
    setp(lc, "volumetric_scattering_intensity", 1.5)

# Faint sky fill: captures the night-sky dome, so moonlit areas never go pure black (Lumen occludes it indoors).
skyl = spawn(unreal.SkyLight, (0, 0, 600), 0, "SkyFill", "Lighting")
slc = skyl.get_component_by_class(unreal.SkyLightComponent)
setp(slc, "mobility", unreal.ComponentMobility.MOVABLE)
setp(slc, "intensity", SKY_FILL)
setp(slc, "light_color", unreal.Color(r=150, g=175, b=255, a=255))
setp(slc, "volumetric_scattering_intensity", 0.5)

# Fog: thin distance haze + volumetric fog for dust in the torch beam and moon shafts.
fog = spawn(unreal.ExponentialHeightFog, (0, 0, 0), 0, "Fog", "Lighting")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
for prop, value in (("fog_density", 0.035), ("fog_height_falloff", 0.35),
                    ("fog_inscattering_luminance", unreal.LinearColor(0.006, 0.008, 0.014, 1.0)),
                    ("fog_max_opacity", 0.9), ("start_distance", 200.0),
                    ("enable_volumetric_fog", True),
                    ("volumetric_fog_scattering_distribution", 0.55),
                    ("volumetric_fog_albedo", unreal.Color(r=235, g=235, b=240, a=255)),
                    ("volumetric_fog_extinction_scale", 5.0),
                    ("volumetric_fog_distance", 3500.0),
                    ("volumetric_fog_start_distance", 0.0)):
    setp(fc, prop, value, "Fog")

ppv = spawn(unreal.PostProcessVolume, (0, 0, 0), 0, "PostProcess_Horror", "Lighting")
ppv.set_editor_property("unbound", True)
settings = ppv.get_editor_property("settings")
V4 = unreal.Vector4
for prop, value in (
        # Fixed exposure: the house must stay dark when you look at the torch spot.
        ("auto_exposure_min_brightness", EXPOSURE_EV100), ("auto_exposure_max_brightness", EXPOSURE_EV100),
        ("local_exposure_highlight_contrast_scale", 0.75), ("local_exposure_shadow_contrast_scale", 1.0),
        # Filmic curve: deep toe, soft shoulder so the torch spot rolls off instead of clipping.
        ("tone_curve_amount", 1.0), ("film_slope", 0.86), ("film_toe", 0.62), ("film_shoulder", 0.30),
        ("film_black_clip", 0.0), ("film_white_clip", 0.04),
        # Grade: desaturated, cool blue-green shadows, warm lamp highlights.
        ("color_saturation", V4(0.86, 0.86, 0.86, 1.0)), ("color_contrast", V4(1.06, 1.06, 1.06, 1.0)),
        ("color_gain_shadows", V4(0.86, 1.0, 1.10, 1.0)), ("color_saturation_shadows", V4(0.9, 0.9, 0.9, 1.0)),
        ("color_offset_shadows", V4(0.0, 0.0015, 0.003, 0.0)),
        ("color_gain_highlights", V4(1.08, 1.0, 0.88, 1.0)),
        ("color_correction_shadows_max", 0.12), ("color_correction_highlights_min", 0.45),
        ("white_temp", 6300.0),
        # Lens: mild bloom on lamps and flames, slight fringe, strong vignette, subtle grain.
        ("bloom_intensity", 0.8), ("bloom_threshold", -1.0),
        ("scene_fringe_intensity", 0.7), ("chromatic_aberration_start_offset", 0.35),
        ("vignette_intensity", 0.85),
        ("film_grain_intensity", 0.18), ("film_grain_intensity_shadows", 0.6), ("film_grain_intensity_highlights", 0.1),
        ("motion_blur_amount", 0.25),
        # Lumen: cleaner GI and reflections in the dark rooms.
        ("lumen_scene_lighting_quality", 2.0), ("lumen_scene_detail", 1.5), ("lumen_final_gather_quality", 2.0),
        ("lumen_reflection_quality", 1.5)):
    try:
        settings.set_editor_property("override_" + prop, True)
    except Exception:
        pass  # a few settings have no override flag
    try:
        settings.set_editor_property(prop, value)
    except Exception as exc:
        unreal.log_warning(f"Post process: could not set {prop}: {exc}")
ppv.set_editor_property("settings", settings)

level_sub.save_current_level()
unreal.log(f"Haveli built and saved to {MAP_PATH} ({decor_count} decor meshes, {decal_count} decals, {prop_count} props, {breakable_count} breakables). Press Play.")
