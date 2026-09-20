#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
launcher_source="$source_dir/omarchy-launch-screensaver"
config_source="$source_dir/omarchy-screensaver.conf.example"
local_bin="${OMARIDIAN_LOCAL_BIN_DIR:-$HOME/.local/bin}"
config_dir="${XDG_CONFIG_HOME:-$HOME/.config}/omaridian"
launcher_target="$local_bin/omarchy-launch-screensaver"
config_target="$config_dir/omarchy-screensaver.conf"
shell_rc="${OMARIDIAN_SHELL_RC:-$HOME/.bashrc}"
path_begin='# omaridian-omarchy-integration: begin'
path_end='# omaridian-omarchy-integration: end'

if [[ -f "$launcher_target" ]]; then
  if cmp -s "$launcher_source" "$launcher_target"; then
    rm "$launcher_target"
    printf '%s\n' "Removed $launcher_target"
  else
    printf '%s\n' "Left unrelated launcher untouched: $launcher_target"
  fi
fi

if [[ -f "$config_target" ]]; then
  if cmp -s "$config_source" "$config_target"; then
    rm "$config_target"
    printf '%s\n' "Removed default Omaridian config: $config_target"
  else
    printf '%s\n' "Left customized config untouched: $config_target"
  fi
fi

# Only remove directories that are empty; never recursively delete user data.
rmdir "$config_dir" 2>/dev/null || true
rmdir "$local_bin" 2>/dev/null || true

if [[ -f "$shell_rc" ]] && grep -Fqx "$path_begin" "$shell_rc"; then
  tmp_rc="${shell_rc}.omaridian.$$"
  awk -v begin="$path_begin" -v end="$path_end" '
    $0 == begin { skip=1; next }
    skip && $0 == end { skip=0; next }
    skip > 0 { next }
    { print }
  ' "$shell_rc" > "$tmp_rc"
  chmod --reference="$shell_rc" "$tmp_rc" 2>/dev/null || chmod 644 "$tmp_rc"
  mv -f "$tmp_rc" "$shell_rc"
  printf '%s\n' "Removed Omaridian PATH activation from $shell_rc"
fi
