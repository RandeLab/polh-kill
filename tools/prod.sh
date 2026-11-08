#!/usr/bin/env bash
# Push straight to prod: build firmware + web UI and flash both over WiFi (web OTA at /update).
# Ze learned data lives on its own partition and survives. Run from Git Bash or Linux:
#
#   tools/prod.sh                 find ze trap via polh-kill.local or a scan of ze local /24
#   tools/prod.sh 192.168.1.102   use this IP
#   POLH_ADMIN=secret tools/prod.sh   if an admin password is set on ze device
#   tools/prod.sh --ui-only       only ze web UI (no firmware)
set -euo pipefail
cd "$(dirname "$0")/.."
export PYTHONUTF8=1 PYTHONIOENCODING=utf-8   # esptool & friends crash on cp1252 consoles

UI_ONLY=0; IP=""
for a in "$@"; do [[ "$a" == "--ui-only" ]] && UI_ONLY=1 || IP="$a"; done
AUTH=(); [[ -n "${POLH_ADMIN:-}" ]] && AUTH=(-u "admin:$POLH_ADMIN")

status() { curl -s -m 4 "${AUTH[@]}" "http://$1/api/status"; }
# no "| grep -q" here: with pipefail an early grep exit makes curl die of SIGPIPE, pure chaos
is_polh() { local s; s=$(status "$1" 2>/dev/null) || return 1; [[ "$s" == *'"name"'* ]]; }

find_trap() {
  if is_polh polh-kill.local 2>/dev/null; then echo polh-kill.local; return; fi
  local me base
  me=$( (ipconfig 2>/dev/null | grep -oE 'IPv4[^:]*: *[0-9.]+' | grep -oE '[0-9.]+$'; hostname -I 2>/dev/null | tr ' ' '\n') | grep -vE '^(127|172\.(1[6-9]|2|3[01]))\.' | head -1)
  base=${me%.*}
  for i in $(seq 1 254); do (curl -s -m 1 -o /dev/null -w '%{http_code}' "http://$base.$i/api/status" | grep -q 200 && is_polh "$base.$i" && echo "$base.$i") & done | head -1
  wait 2>/dev/null || true
}

wait_up() { for _ in $(seq 1 40); do sleep 2; is_polh "$1" && return 0; done; echo "❌ ze trap did not come back"; exit 1; }
field() { grep -oE "\"$1\":[^,}]*" | head -1 | cut -d: -f2 | tr -d '"'; }

[[ -z "$IP" ]] && IP=$(find_trap)
[[ -z "$IP" ]] && { echo "❌ cannot find ze trap. Give me ze IP: tools/prod.sh 192.168.x.y"; exit 1; }
before=$(status "$IP")
echo "🐭 ze trap at $IP: v$(echo "$before" | field version), $(echo "$before" | field captures) captures"

B=$(pio project config 2>/dev/null | sed -nE 's/^workspace_dir\s*=\s*//p' | head -1 | tr -d '\r')
command -v cygpath >/dev/null && [[ -n "$B" ]] && B=$(cygpath -u "$B")
B="${B:-.pio}/build/polh"
[[ $UI_ONLY == 1 ]] || { echo "🔨 building firmware";  pio run -e polh -s; }
echo "🎨 building web UI";  pio run -e polh -t buildfs -s

# a freshly rebooted trap is still busy with itself (WiFi, mDNS, ze first GitHub push),
# so every upload gets three chances and a loud error instead of a silent death
upload() {
  local what=$1 file=$2 code
  for try in 1 2 3; do
    code=$(curl -s -m 240 -o /dev/null -w '%{http_code}' "${AUTH[@]}" -F "update=@$file" "http://$IP/update?type=$what") || code=000
    [[ "$code" == 200 ]] && return 0
    echo "⚠️  $what upload got HTTP $code, try $try/3"; sleep 8
  done
  echo "❌ $what upload failed"; exit 1
}

if [[ $UI_ONLY == 0 ]]; then
  echo "⚡ flashing firmware over WiFi"
  upload fw "$B/firmware.bin"
  wait_up "$IP"; sleep 5
fi
echo "⚡ flashing web UI over WiFi"
upload fs "$B/littlefs.bin"
wait_up "$IP"

# a just-booted trap may still miss one status call (WiFi, TLS to Telegram and GitHub): ask again
after=""; for _ in 1 2 3 4 5; do after=$(status "$IP") && [[ -n "$after" ]] && break; sleep 3; done
cb=$(echo "$before" | field captures); ca=$(echo "$after" | field captures)
echo "✅ now v$(echo "$after" | field version), $ca captures, $(echo "$after" | field labelled) labelled"
if [[ -n "$cb" && -n "$ca" && "$ca" -lt "$cb" ]]; then echo "❌ captures went DOWN ($cb -> $ca)!"; exit 1; fi
echo "And so on, and so on."
