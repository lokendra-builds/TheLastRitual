"""
Creates the 5 ghost data assets from the design doc in /Game/Ghosts.
Skips any ghost asset that already exists, so your edits in the editor are kept.

Run inside the Unreal Editor:  Tools > Execute Python Script...
(build_test_room.py runs this automatically.)
"""
import unreal

FOLDER = "/Game/Ghosts"

GHOSTS = [
    {
        "asset": "Ghost_01_MirrorWoman",
        "id": "MirrorWoman",
        "name": "The Mirror Woman",
        "lore": "She combed her hair at the mirror every night, waiting for someone who never came back.",
        "signs": [
            ("NoReflection", "Mirrors show no reflection"),
            ("CombedHair", "Combed hair on the floor"),
            ("Humming", "Humming at night"),
        ],
        "ritual": "MirrorTrap",
        "hint": "Cover every mirror but one. Trap her in the last.",
    },
    {
        "asset": "Ghost_02_DrownedChild",
        "id": "DrownedChild",
        "name": "The Drowned Child",
        "lore": "The well was sealed after the summer of '87. Something still climbs out.",
        "signs": [
            ("WetFootprints", "Small wet footprints"),
            ("Dripping", "Dripping sounds"),
            ("ColdNearWell", "Cold patches near the well"),
        ],
        "ritual": "SaltCircle",
        "hint": "A circle of salt around the well, then call the child by name.",
    },
    {
        "asset": "Ghost_03_Clockmaker",
        "id": "Clockmaker",
        "name": "The Clockmaker",
        "lore": "He fixed every clock in town. His own stopped at three.",
        "signs": [
            ("ClocksStopAt3", "All clocks stopped at 3:00"),
            ("TickingInWalls", "Ticking inside the walls"),
            ("BrokenWatches", "Broken watches"),
        ],
        "ritual": "TempleBell",
        "hint": "Ring the temple bell three times at 3:00 in the prayer room.",
    },
    {
        "asset": "Ghost_04_BackwardWalker",
        "id": "BackwardWalker",
        "name": "The Backward Walker",
        "lore": "Feet turned the wrong way. It always uses the same door.",
        "signs": [
            ("BackwardFootprints", "Footprints pointing the wrong way"),
            ("DoorsOpenThemselves", "Doors opening by themselves"),
        ],
        "ritual": "IronNail",
        "hint": "Drive an iron nail into the doorway it keeps using.",
    },
    {
        "asset": "Ghost_05_Bride",
        "id": "Bride",
        "name": "The Bride",
        "lore": "The wedding never happened. The music never stopped.",
        "signs": [
            ("RedBangles", "Red bangles"),
            ("WeddingMusic", "Wedding music from nowhere"),
            ("BurningSmell", "A burning smell"),
        ],
        "ritual": "BurnLetter",
        "hint": "Burn her letter in the oil lamp at the shrine.",
    },
]


def create_ghosts():
    ghost_cls = unreal.load_class(None, "/Script/TheLastRitual.GhostDefinition")
    if ghost_cls is None:
        raise RuntimeError("GhostDefinition class not found. Compile the C++ code first (./start.sh build).")

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for g in GHOSTS:
        path = f"{FOLDER}/{g['asset']}"
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log(f"Ghost exists, skipping: {path}")
            continue

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ghost_cls)
        asset = tools.create_asset(g["asset"], FOLDER, ghost_cls, factory)

        asset.set_editor_property("ghost_id", g["id"])
        asset.set_editor_property("display_name", g["name"])
        asset.set_editor_property("lore", g["lore"])
        asset.set_editor_property("signs", [
            unreal.GhostSign(sign_id=sid, description=desc) for sid, desc in g["signs"]
        ])
        asset.set_editor_property("ritual_id", g["ritual"])
        asset.set_editor_property("ritual_hint", g["hint"])

        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        unreal.log(f"Created ghost: {path}")


if __name__ == "__main__":
    create_ghosts()
