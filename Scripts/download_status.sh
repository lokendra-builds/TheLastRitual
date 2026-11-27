#!/usr/bin/env bash
# Shows Unreal Engine download progress as a table, refreshed live.
#
#   ./Scripts/download_status.sh          live view, refreshes every 5 s (Ctrl+C to stop)
#   ./Scripts/download_status.sh once     print once and exit
#
# Total size defaults to 37.1 GB (as shown by Chrome, which uses 1024-based GB).
# Override: TOTAL_GB=37.1 ./Scripts/download_status.sh
set -uo pipefail

TOTAL_GB="${TOTAL_GB:-37.1}"
INTERVAL=5
DL_DIR="$HOME/Downloads"
FINAL_NAME="Linux_Unreal_Engine_5.8.4.zip"

find_file() {
	if [[ -f "$DL_DIR/$FINAL_NAME" ]]; then echo "$DL_DIR/$FINAL_NAME"; return; fi
	ls -t "$DL_DIR"/*.crdownload 2>/dev/null | head -1
}

snapshot() {
	local file="$1" size_now="$2" size_prev="$3" secs="$4" first_size="$5" elapsed="$6"
	python3 - "$file" "$size_now" "$size_prev" "$secs" "$TOTAL_GB" "$first_size" "$elapsed" <<'EOF'
import os, sys, datetime
file, now, prev, secs, total_gb, first, elapsed = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5]), int(sys.argv[6]), float(sys.argv[7])
GiB = 1024 ** 3
total = total_gb * GiB
done = now >= total * 0.999 and file.endswith(".zip")

speed = (now - prev) / secs if secs > 0 else 0
avg = (now - first) / elapsed if elapsed > 0 else speed
left = max(total - now, 0)
eta = left / avg if avg > 0 else None
pct = min(now / total * 100, 100)

if done:
    status = "DONE - ready to unzip"
elif speed > 0:
    status = "Downloading"
else:
    status = "STALLED (no new data)"

def hms(s):
    if s is None: return "unknown"
    s = int(s); return f"{s // 3600}h {s % 3600 // 60:02d}m" if s >= 3600 else f"{s // 60}m {s % 60:02d}s"

finish = (datetime.datetime.now() + datetime.timedelta(seconds=eta)).strftime("%H:%M") if eta else "unknown"
bar_w = 30
bar = "#" * int(pct / 100 * bar_w) + "-" * (bar_w - int(pct / 100 * bar_w))

rows = [
    ("File", os.path.basename(file)),
    ("Status", status),
    ("Progress", f"[{bar}] {pct:.1f}%"),
    ("Downloaded", f"{now / GiB:.2f} GB of {total_gb:.1f} GB"),
    ("Remaining", f"{left / GiB:.2f} GB"),
    ("Speed now", f"{speed / 1024**2:.1f} MB/s"),
    ("Average speed", f"{avg / 1024**2:.1f} MB/s"),
    ("Time left", hms(eta)),
    ("Finishes around", finish),
    ("Checked at", datetime.datetime.now().strftime("%H:%M:%S")),
]
w1 = max(len(r[0]) for r in rows); w2 = max(len(r[1]) for r in rows)
line = f"+-{'-' * w1}-+-{'-' * w2}-+"
print(line)
print(f"| {'Item'.ljust(w1)} | {'Value'.ljust(w2)} |")
print(line)
for k, v in rows:
    print(f"| {k.ljust(w1)} | {v.ljust(w2)} |")
print(line)
EOF
}

file="$(find_file)"
if [[ -z "$file" ]]; then
	echo "No download found in $DL_DIR (no $FINAL_NAME and no .crdownload file)."
	exit 1
fi

first_size=$(stat -c %s "$file")
start=$(date +%s)
prev=$first_size
sleep "$INTERVAL"

while true; do
	file="$(find_file)"
	now=$(stat -c %s "$file" 2>/dev/null || echo "$prev")
	elapsed=$(( $(date +%s) - start ))
	[[ "${1:-}" != "once" ]] && clear
	snapshot "$file" "$now" "$prev" "$INTERVAL" "$first_size" "$elapsed"
	[[ "${1:-}" == "once" ]] && break
	[[ "$file" == *.zip ]] && { echo "Download complete. Next: unzip and run ./start.sh"; break; }
	prev=$now
	sleep "$INTERVAL"
done
