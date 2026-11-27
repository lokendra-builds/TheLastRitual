#!/usr/bin/env bash
# The Last Ritual: one script for setup, build and launch.
#
#   ./start.sh            check -> build -> open the editor (builds the haveli map on first run)
#   ./start.sh check      check engine, disk space and GPU only
#   ./start.sh build      compile the C++ code only
#   ./start.sh editor     open the editor without building
#   ./start.sh game       play the game in a standalone window (no editor UI)
#   ./start.sh room       rebuild the grey-box haveli map (deletes the old one first)
#   ./start.sh clean      delete build output (Binaries, Intermediate) for a fresh build
#   ./start.sh engine     forget the saved engine path and ask again
#
# The engine path is found automatically (UE_ROOT, then ~/UnrealEngine*, ~/Epic*, /opt)
# and remembered in .ue_root, so you only set it once.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_NAME="TheLastRitual"
UPROJECT="$PROJECT_DIR/$PROJECT_NAME.uproject"
UE_ROOT_FILE="$PROJECT_DIR/.ue_root"
TEST_MAP="$PROJECT_DIR/Content/Maps/Haveli_Night1.umap"
ROOM_SCRIPT="$PROJECT_DIR/Scripts/build_haveli.py"
MAP_NAME="/Game/Maps/Haveli_Night1"
MIN_FREE_GB=60   # needed for the project cache and builds once the engine is installed

red()    { printf '\033[31m%s\033[0m\n' "$*"; }
green()  { printf '\033[32m%s\033[0m\n' "$*"; }
yellow() { printf '\033[33m%s\033[0m\n' "$*"; }
step()   { printf '\n\033[1m==> %s\033[0m\n' "$*"; }

# ------------------------------------------------------------------ engine

is_engine_dir() { [[ -x "$1/Engine/Binaries/Linux/UnrealEditor" ]]; }

find_engine() {
	if [[ -n "${UE_ROOT:-}" ]] && is_engine_dir "$UE_ROOT"; then
		echo "$UE_ROOT"; return 0
	fi
	if [[ -f "$UE_ROOT_FILE" ]]; then
		local saved; saved="$(<"$UE_ROOT_FILE")"
		if is_engine_dir "$saved"; then echo "$saved"; return 0; fi
	fi
	local candidate
	for candidate in "$HOME"/UnrealEngine* "$HOME"/UnrealEngine/* "$HOME"/Epic* "$HOME"/Epic/* /opt/UnrealEngine* /opt/unreal*; do
		if [[ -d "$candidate" ]] && is_engine_dir "$candidate"; then
			echo "$candidate"; return 0
		fi
	done
	return 1
}

require_engine() {
	if ! UE_ROOT="$(find_engine)"; then
		red "Unreal Engine not found."
		echo "  1. Download UE5 for Linux: https://www.unrealengine.com/linux (Epic login)"
		echo "  2. Unzip it, e.g. to ~/UnrealEngine/"
		echo "  3. Run again, or point to it: UE_ROOT=/path/to/engine ./start.sh"
		exit 1
	fi
	echo "$UE_ROOT" > "$UE_ROOT_FILE"
	EDITOR_BIN="$UE_ROOT/Engine/Binaries/Linux/UnrealEditor"
	BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh"
	green "Engine: $UE_ROOT"
}

# ------------------------------------------------------------------ steps

do_check() {
	step "Checking your PC"
	local ok=1

	local free_gb need_gb=$MIN_FREE_GB
	free_gb=$(df -BG --output=avail "$PROJECT_DIR" | tail -1 | tr -dc '0-9')
	find_engine >/dev/null || need_gb=150   # engine download + unzip + project
	if (( free_gb < need_gb )); then
		red "Disk: only ${free_gb} GB free (need about ${need_gb} GB)."
		ok=0
	else
		green "Disk: ${free_gb} GB free"
	fi

	if command -v nvidia-smi >/dev/null 2>&1; then
		green "GPU: $(nvidia-smi --query-gpu=name,driver_version --format=csv,noheader 2>/dev/null | head -1)"
	else
		yellow "GPU: nvidia-smi not found, can't check the NVIDIA driver."
	fi

	if command -v vulkaninfo >/dev/null 2>&1; then
		if vulkaninfo --summary >/dev/null 2>&1; then green "Vulkan: OK"; else red "Vulkan: vulkaninfo failed. Unreal on Linux needs Vulkan."; ok=0; fi
	else
		yellow "Vulkan: 'vulkaninfo' not installed (sudo apt install vulkan-tools) — skipping check."
	fi

	if find_engine >/dev/null; then
		green "Engine: $(find_engine)"
	else
		red "Engine: not found (see: ./start.sh engine)"
		ok=0
	fi

	if (( ok )); then green "All good."; else yellow "Fix the red items above."; fi
	return 0
}

do_build() {
	require_engine
	step "Compiling C++ ($PROJECT_NAME Editor, Development)"
	# -NoUBA: the UBA accelerator crashed clang (SIGSEGV in opendir) on this machine after an interrupted build.
	"$BUILD_SH" "${PROJECT_NAME}Editor" Linux Development -project="$UPROJECT" -waitmutex -NoUBA
	green "Build finished."
}

build_room() {
	# -ExecutePythonScript runs the script and then closes the editor, so this is its own step.
	step "Building the haveli map (editor opens, runs the script, closes)"
	"$EDITOR_BIN" "$UPROJECT" -ExecutePythonScript="$ROOM_SCRIPT" -unattended -nosplash >/dev/null 2>&1 || true
	if grep -q "Python script executed with errors" "$PROJECT_DIR/Saved/Logs/$PROJECT_NAME.log" 2>/dev/null; then
		red "The test room script failed. Errors:"
		grep "LogPython: Error" "$PROJECT_DIR/Saved/Logs/$PROJECT_NAME.log" | tail -10
		rm -f "$TEST_MAP"   # half-built map; rebuild next time
		exit 1
	fi
	green "Map built: $MAP_NAME"
}

do_editor() {
	require_engine
	[[ -f "$TEST_MAP" ]] || build_room
	step "Opening the editor"
	"$EDITOR_BIN" "$UPROJECT" "$MAP_NAME" >/dev/null 2>&1 &
	disown
	green "Editor is starting (first launch compiles shaders and can take a long time)."
	echo "Logs: $PROJECT_DIR/Saved/Logs/$PROJECT_NAME.log"
}

do_game() {
	require_engine
	local map_arg=()
	[[ -f "$TEST_MAP" ]] && map_arg=("$MAP_NAME")
	step "Starting the game in a window"
	exec "$EDITOR_BIN" "$UPROJECT" "${map_arg[@]}" -game -windowed -resx=1600 -resy=900 -log
}

do_room() {
	if [[ -f "$TEST_MAP" ]]; then
		yellow "Deleting old test room: $TEST_MAP"
		rm -f "$TEST_MAP"
	fi
	do_editor
}

do_clean() {
	step "Cleaning build output"
	rm -rf "$PROJECT_DIR/Binaries" "$PROJECT_DIR/Intermediate"
	green "Removed Binaries/ and Intermediate/. Next build will be a full rebuild."
}

# ------------------------------------------------------------------ main

cmd="${1:-all}"
case "$cmd" in
	all)    do_check; do_build; do_editor ;;
	check)  do_check ;;
	build)  do_build ;;
	editor) do_editor ;;
	game)   do_game ;;
	room)   do_room ;;
	clean)  do_clean ;;
	engine) rm -f "$UE_ROOT_FILE"; unset UE_ROOT; require_engine ;;
	-h|--help|help) sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//' ;;
	*) red "Unknown command: $cmd"; sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'; exit 1 ;;
esac
