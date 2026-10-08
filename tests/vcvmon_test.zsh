set -eu
source scripts/vcvmon.zsh
local_root=$(mktemp -d /tmp/vcvmon-test.XXXXXX)
trap '[[ $ZSH_SUBSHELL == 0 ]] && rm -rf "$local_root"' EXIT
mkdir -p "$local_root/repo" "$local_root/sdk" "$local_root/dev" "$local_root/Rack.app/Contents/MacOS"
print '{}' > "$local_root/repo/plugin.json"
print '' > "$local_root/sdk/plugin.mk"
print '#!/bin/zsh\nprint launched >> "$VCVMON_TEST_LOG"' > "$local_root/Rack.app/Contents/MacOS/Rack"
chmod +x "$local_root/Rack.app/Contents/MacOS/Rack"
export VCVMON_TEST_LOG="$local_root/log"
VCVMON_REPO="$local_root/repo"
VCVMON_RACK="$local_root/dev"
VCVMON_SDK="$local_root/sdk"
VCVMON_APP="$local_root/Rack.app"
VCVMON_USER_DIR="$local_root/user"
VCVMON_PAUSE="$local_root/pause"
make () {
  print -r -- "$PWD $*" >> "$local_root/make-log"
  [[ ${fail_build:-0} == 1 ]] && return 1
  if [[ "$*" == *dist* ]]; then
    mkdir -p "$VCVMON_REPO/dist"
    print package > "$VCVMON_REPO/dist/computerscare-1-mac-arm64.vcvplugin"
  fi
  return 0
}
uname () { print arm64; }
ps () { return 0; }
read () {
  if [[ "$*" == *-sk1* ]]; then sleep .2;key=q;return 0;fi
  builtin read "$@"
}
vcvmon dist > "$local_root/dist-log" 2>&1
[[ -f "$VCVMON_USER_DIR/plugins-mac-arm64/computerscare-1-mac-arm64.vcvplugin" ]]
sleep .1
[[ -f "$local_root/log" ]]
[[ $(cat "$local_root/make-log") == *"RACK_DIR=$VCVMON_SDK dist"* ]]
rm "$local_root/log" "$local_root/make-log"
fail_build=1
vcvmon dist > "$local_root/fail-log" 2>&1
[[ ! -f "$local_root/log" ]]
[[ $(cat "$local_root/fail-log") == *'BUILD FAILED'* ]]
fail_build=0
vcvmon dev > "$local_root/dev-log" 2>&1
sleep .1
[[ $(cat "$local_root/make-log") == *"$VCVMON_RACK plugins run"* ]]
if vcvmon invalid > /dev/null 2>&1; then exit 1;fi
vcvmon > "$local_root/default-log" 2>&1
if vcvmon dev extra > /dev/null 2>&1; then exit 1;fi
print 'vcvmon mode and build-failure tests passed' 
