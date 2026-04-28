#!/usr/bin/env bash
set -euo pipefail

zero_sha="0000000000000000000000000000000000000000"
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "$script_dir/.." && pwd)
plugin_json="plugin.json"

cd "$repo_root"

if [[ ! -f "$plugin_json" ]]; then
  echo "pre-push: $plugin_json not found" >&2
  exit 1
fi

plugin_version=$(
  awk -F'"' '/"version"[[:space:]]*:/ { print $4; exit }' "$plugin_json"
)

if [[ -z "$plugin_version" ]]; then
  echo "pre-push: could not read version from $plugin_json" >&2
  exit 1
fi

status=0

while read -r local_ref local_sha remote_ref remote_sha; do
  if [[ -z "${local_ref:-}" ]]; then
    continue
  fi

  # Tag deletion pushes use an all-zero local SHA. Do not block deletions.
  if [[ "$local_sha" == "$zero_sha" ]]; then
    continue
  fi

  if [[ "$remote_ref" =~ ^refs/tags/(v[0-9]+([.][0-9]+)*)$ ]]; then
    tag="${BASH_REMATCH[1]}"
    tag_version="${tag#v}"

    if [[ "$tag_version" != "$plugin_version" ]]; then
      echo "pre-push: refusing to push tag $tag" >&2
      echo "pre-push: $plugin_json version is $plugin_version" >&2
      echo "pre-push: update $plugin_json or push tag v$plugin_version" >&2
      status=1
    fi
  fi
done

exit "$status"
