#!/usr/bin/env bash
# Takes an in-game screenshot of the test room for lighting checks.
#   ./Scripts/shot.sh [name] [seconds] [yaw] [pitch] [x] [y]     (EXTRA="-FastHunts" for extra game args)
# Saves to Saved/Shots/<name>.png
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="${UE_ROOT:-$(cat "$PROJECT_DIR/.ue_root")}"
NAME="${1:-shot}"; DELAY="${2:-8}"; YAW="${3:-}"; PITCH="${4:-}"; X="${5:-}"; Y="${6:-}"

args=(-AutoShot="$DELAY")
[[ -n "$YAW" ]] && args+=(-AutoShotYaw="$YAW")
[[ -n "$PITCH" ]] && args+=(-AutoShotPitch="$PITCH")
[[ -n "$X" && -n "$Y" ]] && args+=(-AutoShotX="$X" -AutoShotY="$Y")
# shellcheck disable=SC2206
args+=(${EXTRA:-})

# One screenshot run at a time (several agents may call this).
mkdir -p "$PROJECT_DIR/Saved"
exec 9>"$PROJECT_DIR/Saved/.shot.lock"
flock 9

rm -rf "$PROJECT_DIR/Saved/Screenshots"
timeout $((DELAY + 60)) "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" "$PROJECT_DIR/TheLastRitual.uproject" \
	"${MAP:-/Game/Maps/Haveli_Night1}" -game -windowed -resx=1280 -resy=720 -nosound -unattended -log=ShotTest.log "${args[@]}" \
	>/dev/null 2>&1 || true

mkdir -p "$PROJECT_DIR/Saved/Shots"
src="$(find "$PROJECT_DIR/Saved/Screenshots" -name '*.png' | head -1)"
[[ -n "$src" ]] || { echo "No screenshot produced. See Saved/Logs/ShotTest.log"; exit 1; }
mv "$src" "$PROJECT_DIR/Saved/Shots/$NAME.png"
echo "$PROJECT_DIR/Saved/Shots/$NAME.png"
