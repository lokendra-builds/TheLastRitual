"""
Builds the Week 1 grey-box test room: /Game/Maps/TestRoom

Run inside the Unreal Editor (after the C++ code compiles):
    Tools > Execute Python Script... > pick this file
or in the editor console (`):
    py "<full path>/Scripts/build_test_room.py"

Layout (top view, cm). Room A is dark (torch only). Room B has a dim oil-lamp light.

          y=1000  +-----------------+
                  |     Room B      |   battery in the corner
                  |   (oil lamp)    |
          y=400   +------[door]-----+
                  |     Room A      |
                  |   player start  |
          y=-400  +-----------------+
               x=-400             x=400
"""
import os
import runpy

import unreal

# Make sure the ghost data assets exist first.
runpy.run_path(os.path.join(unreal.Paths.project_dir(), "Scripts", "create_ghosts.py"), run_name="__main__")

MAP_PATH = "/Game/Maps/TestRoom"
W = 800.0      # room width (x)
H = 300.0      # wall height
T = 20.0       # wall thickness
Y_MIN, Y_MID, Y_MAX = -400.0, 400.0, 1000.0
DOOR_W, DOOR_H = 100.0, 210.0

# Lighting knobs (tuned from screenshots). EXPOSURE_EV100: lower = brighter picture.
MOON_LUX = 2.0
LAMP_CANDELA = 40.0
EXPOSURE_EV100 = 2.5

level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    raise RuntimeError(f"{MAP_PATH} already exists. Delete it first if you want to rebuild it.")

level_sub.new_level(MAP_PATH)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")


def box(label, x0, x1, y0, y1, z0, z1):
    """Axis-aligned box from min/max corners (cm), using the 100cm engine cube."""
    center = unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, center)
    actor.set_actor_label(label)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.set_actor_scale3d(unreal.Vector((x1 - x0) / 100, (y1 - y0) / 100, (z1 - z0) / 100))
    actor.set_folder_path("Greybox")
    return actor


x0, x1 = -W / 2, W / 2

# Floor and ceiling over both rooms.
box("Floor", x0 - T, x1 + T, Y_MIN - T, Y_MAX + T, -T, 0)
box("Ceiling", x0 - T, x1 + T, Y_MIN - T, Y_MAX + T, H, H + T)

# Outer walls.
# South wall with a window (jharokha stand-in) so moonlight gets into room A.
WIN_X0, WIN_X1, WIN_Z0, WIN_Z1 = -80, 80, 110, 230
box("Wall_South_Left", x0 - T, WIN_X0, Y_MIN - T, Y_MIN, 0, H)
box("Wall_South_Right", WIN_X1, x1 + T, Y_MIN - T, Y_MIN, 0, H)
box("Wall_South_Below", WIN_X0, WIN_X1, Y_MIN - T, Y_MIN, 0, WIN_Z0)
box("Wall_South_Above", WIN_X0, WIN_X1, Y_MIN - T, Y_MIN, WIN_Z1, H)
# Window bars.
for bx in (-40, 0, 40):
    box(f"Window_Bar_{bx}", bx - 2, bx + 2, Y_MIN - T / 2 - 2, Y_MIN - T / 2 + 2, WIN_Z0, WIN_Z1)

# Moon: low, cold, coming in through the window.
moon = actor_sub.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, -1500, 1000), unreal.Rotator(0, -25, 70))
moon.set_actor_label("Moonlight")
moon_comp = moon.get_component_by_class(unreal.DirectionalLightComponent)
moon_comp.set_editor_property("intensity", MOON_LUX)
moon_comp.set_editor_property("light_color", unreal.Color(r=150, g=175, b=255, a=255))
moon_comp.set_editor_property("atmosphere_sun_light", False)

box("Wall_North", x0 - T, x1 + T, Y_MAX, Y_MAX + T, 0, H)
box("Wall_West", x0 - T, x0, Y_MIN, Y_MAX, 0, H)
box("Wall_East", x1, x1 + T, Y_MIN, Y_MAX, 0, H)

# Middle wall with a door gap at x = -50..50.
half = DOOR_W / 2
box("Wall_Mid_Left", x0, -half, Y_MID - T / 2, Y_MID + T / 2, 0, H)
box("Wall_Mid_Right", half, x1, Y_MID - T / 2, Y_MID + T / 2, 0, H)
box("Wall_Mid_Top", -half, half, Y_MID - T / 2, Y_MID + T / 2, DOOR_H, H)

# A table-ish block in room B so the battery isn't just on the floor.
box("Table", 250, 350, 850, 950, 0, 75)

# Door: hinge at x=-50, door extends along local +Y; yaw -90 turns that into world +X.
door_cls = unreal.load_class(None, "/Script/TheLastRitual.HouseDoor")
door = actor_sub.spawn_actor_from_class(door_cls, unreal.Vector(-half, Y_MID, 0), unreal.Rotator(0, 0, -90))
door.set_actor_label("Door_AtoB")

battery_cls = unreal.load_class(None, "/Script/TheLastRitual.BatteryPickup")
battery = actor_sub.spawn_actor_from_class(battery_cls, unreal.Vector(300, 900, 79))
battery.set_actor_label("Battery_01")

# Clues for tonight's ghost (The Mirror Woman) + one red herring (Drowned Child).
clue_cls = unreal.load_class(None, "/Script/TheLastRitual.ClueActor")


def clue(label, location, scale, sign_id, name, note, clue_type, red_herring=False, pick_up=False):
    actor = actor_sub.spawn_actor_from_class(clue_cls, unreal.Vector(*location))
    actor.set_actor_label(label)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_folder_path("Clues")
    actor.set_editor_property("pick_up", pick_up)
    comp = actor.get_component_by_class(unreal.ClueComponent)
    comp.set_editor_property("sign_id", sign_id)
    comp.set_editor_property("display_name", name)
    comp.set_editor_property("description", note)
    comp.set_editor_property("clue_type", clue_type)
    comp.set_editor_property("is_red_herring", red_herring)
    return actor


# Mirrors: the first one is also a clue. Ritual = cover one, trap her in the other.
mirror_cls = unreal.load_class(None, "/Script/TheLastRitual.MirrorActor")


def mirror(label, location, yaw, sign_id="", name="", note=""):
    actor = actor_sub.spawn_actor_from_class(mirror_cls, unreal.Vector(*location), unreal.Rotator(0, 0, yaw))
    actor.set_actor_label(label)
    actor.set_folder_path("Mirrors")
    if sign_id:
        comp = actor.get_component_by_class(unreal.ClueComponent)
        comp.set_editor_property("sign_id", sign_id)
        comp.set_editor_property("display_name", name)
        comp.set_editor_property("description", note)
        comp.set_editor_property("clue_type", unreal.ClueType.VISUAL)
    return actor


mirror("Mirror_RoomA", (-397, 0, 150), 0, "NoReflection", "Old mirror", "My torch shows in it. I don't.")
mirror("Mirror_RoomB", (397, 650, 150), 180)

# Other clues. Placeholder shapes (base clue mesh is 20 x 20 x 5 cm).
clue("Clue_Hair", (150, -100, 1), (1.0, 1.0, 0.1), "CombedHair", "Combed hair",
     "Long black hair, neatly combed. On the floor.", unreal.ClueType.VISUAL)
clue("Clue_Diary", (300, 880, 76), (1.0, 1.4, 0.1), "Humming", "Diary page",
     "'Every night she hums the same song. Ma says not to answer.'", unreal.ClueType.WRITTEN, pick_up=True)
clue("Clue_WetFootprints", (-100, 600, 1), (3.0, 1.0, 0.05), "WetFootprints", "Wet footprints",
     "Small and wet. Leading nowhere.", unreal.ClueType.VISUAL, red_herring=True)

# A wrong ritual to fall for: the red herring points at the Drowned Child, whose ritual is the salt circle.
salt_cls = unreal.load_class(None, "/Script/TheLastRitual.RitualItemPickup")
salt = actor_sub.spawn_actor_from_class(salt_cls, unreal.Vector(-300, -300, 8))
salt.set_actor_label("Item_Salt")
salt.set_folder_path("Ritual")
salt.set_editor_property("item_id", "Salt")
salt.set_editor_property("display_name", "a bag of salt")

spot_cls = unreal.load_class(None, "/Script/TheLastRitual.RitualSpot")
spot = actor_sub.spawn_actor_from_class(spot_cls, unreal.Vector(-250, 800, 1))
spot.set_actor_label("RitualSpot_SaltCircle")
spot.set_folder_path("Ritual")
spot.set_editor_property("ritual_id", "SaltCircle")
spot.set_editor_property("action_text", "Pour a salt circle (stand-in for the well)")
spot.set_editor_property("required_items", ["Salt"])
spot.set_editor_property("required_items_text", "salt")

# Places the ghost can show itself when angry.
points = []
for i, loc in enumerate([(0, 380, 0), (300, 700, 0), (-300, 150, 0)]):
    tp = actor_sub.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(*loc))
    tp.set_actor_label(f"GhostAppear_{i + 1}")
    tp.set_folder_path("Ghost")
    points.append(tp)

ghost_cls = unreal.load_class(None, "/Script/TheLastRitual.GhostBase")
ghost = actor_sub.spawn_actor_from_class(ghost_cls, unreal.Vector(0, 700, 0))
ghost.set_actor_label("Ghost")
ghost.set_folder_path("Ghost")
ghost.set_editor_property("appear_points", points)

# Tonight: The Mirror Woman. Shorter pacing so the test room gets scary quickly.
setup_cls = unreal.load_class(None, "/Script/TheLastRitual.NightSetup")
setup = actor_sub.spawn_actor_from_class(setup_cls, unreal.Vector(0, -300, 200))
setup.set_actor_label("NightSetup")
setup.set_editor_property("tonight_ghost", unreal.load_asset("/Game/Ghosts/Ghost_01_MirrorWoman"))
setup.set_editor_property("restless_after_seconds", 90.0)
setup.set_editor_property("calm_event_interval", unreal.Vector2D(20.0, 40.0))

start = actor_sub.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, -250, 100), unreal.Rotator(0, 0, 90))
start.set_actor_label("PlayerStart")

# Oil-lamp placeholder in room B: warm, weak point light.
lamp = actor_sub.spawn_actor_from_class(unreal.PointLight, unreal.Vector(300, 900, 110))
lamp.set_actor_label("OilLamp_Light")
lamp_comp = lamp.get_component_by_class(unreal.PointLightComponent)
lamp_comp.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
lamp_comp.set_editor_property("intensity", LAMP_CANDELA)
lamp_comp.set_editor_property("light_color", unreal.Color(r=255, g=147, b=41, a=255))
lamp_comp.set_editor_property("attenuation_radius", 700.0)

# Volumetric fog: dust in the torch beam.
fog = actor_sub.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
fog.set_actor_label("Fog")
fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fog_comp.set_editor_property("enable_volumetric_fog", True)
fog_comp.set_editor_property("fog_density", 0.02)

# Post process: fixed exposure (no auto-brightening of the dark), grain, vignette.
ppv = actor_sub.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
ppv.set_actor_label("PostProcess_Horror")
ppv.set_editor_property("unbound", True)
settings = ppv.get_editor_property("settings")
for prop, value in [
    ("auto_exposure_min_brightness", EXPOSURE_EV100),
    ("auto_exposure_max_brightness", EXPOSURE_EV100),
    ("vignette_intensity", 0.6),
    ("film_grain_intensity", 0.25),
]:
    try:
        settings.set_editor_property("override_" + prop, True)
        settings.set_editor_property(prop, value)
    except Exception as exc:  # property names shift a little between engine versions
        unreal.log_warning(f"Post process: could not set {prop}: {exc}")
ppv.set_editor_property("settings", settings)

level_sub.save_current_level()
unreal.log(f"Test room built and saved to {MAP_PATH}. Press Play.")
