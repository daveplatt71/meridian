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

if [[ ! -f "$launcher_source" || ! -f "$config_source" ]]; then
  printf '%s\n' "Omaridian Omarchy integration files are incomplete in $source_dir" >&2
  exit 1
fi

if [[ -L "$launcher_target" || ( -e "$launcher_target" && ! -f "$launcher_target" ) ]]; then
  printf '%s\n' "Refusing to replace non-file: $launcher_target" >&2
  exit 1
fi
if [[ -f "$launcher_target" ]] && ! cmp -s "$launcher_source" "$launcher_target"; then
  printf '%s\n' "Refusing to overwrite an existing launcher: $launcher_target" >&2
  printf '%s\n' "Move it aside or remove it intentionally, then rerun this installer." >&2
  exit 1
fi

install -d -m 755 "$local_bin"
install -m 755 "$launcher_source" "$launcher_target"

if [[ -L "$config_target" || ( -e "$config_target" && ! -f "$config_target" ) ]]; then
  printf '%s\n' "Refusing to replace non-file: $config_target" >&2
  exit 1
fi
if [[ ! -e "$config_target" ]]; then
  install -d -m 755 "$config_dir"
  install -m 644 "$config_source" "$config_target"
fi

# Omarchy's idle service runs `bash -lc`; its stock bootstrap appends
# ~/.local/bin after /usr/bin. Add a small managed block before bash's
# non-interactive early return so the override is actually reachable.
if [[ -e "$shell_rc" && ! -f "$shell_rc" ]]; then
  printf '%s\n' "Refusing to replace non-file: $shell_rc" >&2
  exit 1
fi
if ! grep -Fqx "$path_begin" "$shell_rc" 2>/dev/null; then
  install -d -m 755 "$(dirname "$shell_rc")"
  tmp_rc="${shell_rc}.omaridian.$$"
  if [[ -f "$shell_rc" ]]; then
    awk -v begin="$path_begin" -v end="$path_end" '
      !inserted && $0 ~ /^\[\[ \$- != \*i\* \]\] && return/ {
        print begin
        print "case \":$PATH:\" in *:\"$HOME/.local/bin:\"*) ;; *) PATH=\"$HOME/.local/bin:$PATH\" ;; esac"
        print "export PATH"
        print end
        inserted=1
      }
      { print }
      END {
        if (!inserted) {
          print begin
          print "case \":$PATH:\" in *:\"$HOME/.local/bin:\"*) ;; *) PATH=\"$HOME/.local/bin:$PATH\" ;; esac"
          print "export PATH"
          print end
        }
      }
    ' "$shell_rc" > "$tmp_rc"
  else
    printf '%s\n' "$path_begin" 'export PATH="$HOME/.local/bin:$PATH"' "$path_end" > "$tmp_rc"
  fi
  chmod --reference="$shell_rc" "$tmp_rc" 2>/dev/null || chmod 644 "$tmp_rc"
  mv -f "$tmp_rc" "$shell_rc"
fi

printf '%s\n' "Installed Omaridian's user-owned Omarchy launcher: $launcher_target"
printf '%s\n' "Configuration: $config_target"
printf '%s\n' "PATH activation added to $shell_rc; new Omarchy idle launches will use Omaridian."
printf '%s\n' "Set OMARIDIAN_OMARCHY_ENABLED=0 in the config to return to the stock launcher."
