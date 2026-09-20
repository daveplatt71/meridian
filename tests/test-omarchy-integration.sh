#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
adapter="$repo_dir/integration/omarchy/omarchy-launch-screensaver"
installer="$repo_dir/integration/omarchy/install.sh"
uninstaller="$repo_dir/integration/omarchy/uninstall.sh"
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/omaridian-omarchy-test.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

bin_dir="$tmp_dir/bin"
home_dir="$tmp_dir/home"
mkdir -p "$bin_dir" "$home_dir"
log_file="$tmp_dir/launcher.log"
config_file="$tmp_dir/adapter.conf"
marker_file="$tmp_dir/omaridian-launched"

cat > "$bin_dir/omaridian" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
cat > "$bin_dir/omarchy-toggle-enabled" <<'EOF'
#!/usr/bin/env bash
exit 1
EOF
cat > "$bin_dir/omarchy-hyprland-monitor-focused" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' HDMI-A-1
EOF
cat > "$bin_dir/hyprctl" <<'EOF'
#!/usr/bin/env bash
if [[ "$1 ${2:-}" == "monitors -j" ]]; then
  printf '%s\n' '[{"name":"HDMI-A-1"},{"name":"DP-1"}]'
else
  printf '%s\n' "$*" >> "$OMARIDIAN_TEST_LOG"
fi
EOF
cat > "$bin_dir/jq" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' HDMI-A-1 DP-1
EOF
cat > "$bin_dir/socat" <<'EOF'
#!/usr/bin/env bash
touch "$OMARIDIAN_TEST_MARKER"
printf '%s\n' \
  'openwindow>>0x1,org.omarchy.screensaver,Omaridian' \
  'openwindow>>0x2,org.omarchy.screensaver,Omaridian'
EOF
cat > "$bin_dir/pgrep" <<'EOF'
#!/usr/bin/env bash
if [[ -f "$OMARIDIAN_TEST_MARKER" ]]; then exit 0; fi
exit 1
EOF
cat > "$bin_dir/pkill" <<'EOF'
#!/usr/bin/env bash
printf 'pkill %s\n' "$*" >> "$OMARIDIAN_TEST_LOG"
exit 0
EOF
chmod +x "$bin_dir"/*

cat > "$config_file" <<EOF
OMARIDIAN_BIN=$bin_dir/omaridian
OMARIDIAN_FALLBACK=$bin_dir/stock-launcher
OMARIDIAN_SCREENSAVER_DEADLINE=1
EOF
cat > "$bin_dir/stock-launcher" <<'EOF'
#!/usr/bin/env bash
printf 'stock %s\n' "$*" >> "$OMARIDIAN_TEST_LOG"
EOF
chmod +x "$bin_dir/stock-launcher"

PATH="$bin_dir:/usr/bin:/bin" \
HOME="$home_dir" \
OMARIDIAN_TEST_LOG="$log_file" \
OMARIDIAN_TEST_MARKER="$marker_file" \
OMARIDIAN_OMARCHY_CONFIG="$config_file" \
XDG_RUNTIME_DIR="$tmp_dir/runtime" \
HYPRLAND_INSTANCE_SIGNATURE=test \
  "$adapter" force

grep -F -- '--screensaver' "$log_file"
grep -F -- '--app-id=org.omarchy.screensaver' "$log_file"
grep -F -- '--omarchy-adapter' "$log_file"
grep -F -- '--monitor HDMI-A-1' "$log_file"
grep -F -- '--monitor DP-1' "$log_file"
grep -F -- 'hl.dsp.focus' "$log_file"
if grep -q '^stock ' "$log_file"; then
  printf '%s\n' 'unexpected stock fallback during successful launch' >&2
  exit 1
fi

cat > "$config_file" <<EOF
OMARIDIAN_BIN=$tmp_dir/not-installed
OMARIDIAN_FALLBACK=$bin_dir/stock-launcher
EOF
PATH="$bin_dir:/usr/bin:/bin" \
HOME="$home_dir" \
OMARIDIAN_TEST_LOG="$log_file" \
OMARIDIAN_OMARCHY_CONFIG="$config_file" \
  "$adapter" force
grep -F -- 'stock force' "$log_file"

HOME="$home_dir" XDG_CONFIG_HOME="$home_dir/.config" \
  "$installer"
test -x "$home_dir/.local/bin/omarchy-launch-screensaver"
test -f "$home_dir/.config/omaridian/omarchy-screensaver.conf"
grep -F 'omaridian-omarchy-integration: begin' "$home_dir/.bashrc"
grep -F 'omaridian-omarchy-integration: end' "$home_dir/.bashrc"
printf '%s\n' '[[ -f ~/.bashrc ]] && . ~/.bashrc' > "$home_dir/.bash_profile"
PATH="$bin_dir:/usr/bin:/bin" HOME="$home_dir" bash -lc 'test "$(command -v omarchy-launch-screensaver)" = "$HOME/.local/bin/omarchy-launch-screensaver"'
HOME="$home_dir" XDG_CONFIG_HOME="$home_dir/.config" \
  "$uninstaller"
test ! -e "$home_dir/.local/bin/omarchy-launch-screensaver"
test ! -e "$home_dir/.config/omaridian/omarchy-screensaver.conf"
if grep -Fq 'omaridian-omarchy-integration' "$home_dir/.bashrc"; then
  printf '%s\n' 'uninstaller left PATH activation behind' >&2
  exit 1
fi

# Static checks cover the parts that require a live Hyprland session at runtime.
grep -F 'org.omarchy.screensaver' "$repo_dir/src/main.cpp"
grep -F 'setScreen(selected)' "$repo_dir/src/main.cpp"
grep -F 'idle.screensaver' "$repo_dir/integration/omarchy/README.md"
grep -F '/usr/share/omarchy' "$repo_dir/integration/omarchy/README.md"

printf '%s\n' 'Omarchy adapter, fallback, installer, and uninstaller tests pass'
