#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
launcher_source="$source_dir/omarchy-launch-screensaver"
config_source="$source_dir/omarchy-screensaver.conf.example"
local_bin="${MERIDIAN_LOCAL_BIN_DIR:-$HOME/.local/bin}"
config_dir="${XDG_CONFIG_HOME:-$HOME/.config}/meridian"
launcher_target="$local_bin/omarchy-launch-screensaver"
config_target="$config_dir/omarchy-screensaver.conf"
shell_rc="${MERIDIAN_SHELL_RC:-$HOME/.bashrc}"
path_marker='# meridian-omarchy-integration: prepend user launcher path'

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
    printf '%s\n' "Removed default Meridian config: $config_target"
  else
    printf '%s\n' "Left customized config untouched: $config_target"
  fi
fi

# Only remove directories that are empty; never recursively delete user data.
rmdir "$config_dir" 2>/dev/null || true
rmdir "$local_bin" 2>/dev/null || true

if [[ -f "$shell_rc" ]] && grep -Fqx "$path_marker" "$shell_rc"; then
  tmp_rc="${shell_rc}.meridian.$$"
  awk -v marker="$path_marker" '
    $0 == marker { skip=3; next }
    skip > 0 { skip--; next }
    { print }
  ' "$shell_rc" > "$tmp_rc"
  chmod --reference="$shell_rc" "$tmp_rc" 2>/dev/null || chmod 644 "$tmp_rc"
  mv -f "$tmp_rc" "$shell_rc"
  printf '%s\n' "Removed Meridian PATH activation from $shell_rc"
fi
