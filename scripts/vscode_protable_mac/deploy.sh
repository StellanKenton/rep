#!/bin/zsh

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
default_config="$script_dir/project.json"

if [[ $# -eq 0 && -f "$default_config" ]]; then
  exec python3 "$script_dir/deploy.py" --config "$default_config"
fi

exec python3 "$script_dir/deploy.py" "$@"