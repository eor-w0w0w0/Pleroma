#!/usr/bin/env bash
set -euo pipefail

BIN_DEFAULT="/home/bocor/.local/share/Steam/steamapps/common/Warhammer 40,000 DARKTIDE/binaries/Darktide.exe"
BIN="${1:-$BIN_DEFAULT}"

if [[ ! -f "$BIN" ]]; then
  echo "binary not found: $BIN" >&2
  exit 1
fi

need() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "missing required tool: $1" >&2
    exit 1
  }
}

need strings
need objdump
need llvm-objdump
need rg

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

SECTIONS="$TMP_DIR/sections.txt"
DISASM="$TMP_DIR/disasm.txt"
STRINGS="$TMP_DIR/strings.txt"

objdump -h "$BIN" > "$SECTIONS"
llvm-objdump -d --no-show-raw-insn --print-imm-hex "$BIN" > "$DISASM"
strings -t x "$BIN" > "$STRINGS"

# Darktide.exe is linked at 0x140000000 in all builds we've seen.
IMAGE_BASE_HEX="0x140000000"
IMAGE_BASE_DEC=$((IMAGE_BASE_HEX))

hex_to_dec() {
  printf '%d' "$((16#${1#0x}))"
}

fileoff_to_va() {
  local off_hex="$1"
  local off_dec
  off_dec="$(hex_to_dec "0x$off_hex")"

  awk -v off="$off_dec" '
    /^[[:space:]]*[0-9]+[[:space:]]/ {
      size = strtonum("0x" $3)
      vma = strtonum("0x" $4)
      fileoff = strtonum("0x" $6)
      if (off >= fileoff && off < fileoff + size) {
        printf "0x%X", vma + (off - fileoff)
        found = 1
        exit
      }
    }
    END {
      if (!found) {
        exit 1
      }
    }
  ' "$SECTIONS"
}

print_anchor() {
  local label="$1"
  local pattern="$2"
  local off
  off="$(rg -m1 "$pattern" "$STRINGS" | awk '{print $1}')"
  if [[ -z "${off:-}" ]]; then
    echo "$label [absent]"
    return
  fi
  local va
  va="$(fileoff_to_va "$off")"
  echo "$label @ $va (fileoff 0x${off^^})"
}

echo "== String Anchors =="
print_anchor "join_lan_lobby" 'join_lan_lobby$'
print_anchor "leave_lan_lobby" 'leave_lan_lobby$'
print_anchor "create_lobby_browser" 'create_lobby_browser$'
print_anchor "destroy_lobby_browser" 'destroy_lobby_browser$'
print_anchor "create_lan_lobby" 'create_lan_lobby$'
print_anchor "make_game_session_host" 'make_game_session_host$'
print_anchor "set_game_session_host" 'set_game_session_host$'
print_anchor "lobby_host" 'lobby_host$'
print_anchor "discover_lobby" 'discover_lobby$'
print_anchor "discover_lobby_reply" 'discover_lobby_reply$'
print_anchor "request_connection_reply" 'request_connection_reply$'
print_anchor "lobby_request_join" 'lobby_request_join$'
print_anchor "lobby_request_join_reply" 'lobby_request_join_reply$'

echo
echo "== Known Function RVAs (search hits) =="
for va in \
  0x14048A250 \
  0x14048A580 \
  0x14048AF20 \
  0x14048B0D0 \
  0x14048A660 \
  0x140351390 \
  0x14056E550 \
  0x14056E740 \
  0x14056EED0 \
  0x14056F610 \
  0x14056F630 \
  0x14056FA20 \
  0x14056FA40 \
  0x14056C740 \
  0x14056CA70 \
  0x14056CE00 \
  0x14056CE60 \
  0x14056D1B0 \
  0x14056D210 \
  0x14056D7B0 \
  0x14056E130 \
  0x14056E490 \
  0x14056EBC6 \
  0x14056EFF0 \
  0x14056F2E0 \
  0x14056FBC0 \
  0x140587380 \
  0x1406C5690 \
  0x1406C5780 \
  0x1403383A0 \
  0x140338C22
do
  if rg -q "^${va#0x}:" "$DISASM"; then
    echo "$va PRESENT"
  else
    echo "$va ABSENT"
  fi
done

echo
echo "== Suggested Manual Checks =="
cat <<'EOF'
- If string anchors moved but are still present, re-scan disassembly around their xrefs.
- If a known RVA is ABSENT, search the nearby cluster in the updated binary before trusting old headers.
- Browser/transport globals should be re-confirmed from create_wan_client/join_lan_lobby wrappers, not assumed.
EOF
