# vcvmon - watch this repo, rebuild + run Rack on changes, with keyboard pause/resume.
# Source from ~/.zshrc, for example:
#   source ~/dev/computerscare-vcv-modules/scripts/vcvmon.zsh
#
# Modes: vcvmon (or vcvmon dev) uses the source checkout; vcvmon dist
# builds this plugin against the SDK and runs the installed Rack app.
#
# Optional overrides:
#   VCVMON_REPO=/path/to/plugin/repo
#   VCVMON_RACK=/path/to/VCV-Rack/Rack
#   VCVMON_PAUSE=/tmp/vcvmon.pause
#   VCVMON_SDK=/path/to/Rack-SDK
#   VCVMON_APP="/Applications/VCV Rack 2 Pro.app"
#   VCVMON_USER_DIR="$HOME/Library/Application Support/Rack2"

: ${VCVMON_PAUSE:=/tmp/vcvmon.pause}
: ${VCVMON_RACK:=$HOME/dev/VCV-Rack/Rack}
: ${VCVMON_VERSION:=polling-v3}
: ${VCVMON_SDK:=$HOME/dev/VCV-Rack/Rack-SDK}
: ${VCVMON_APP:=/Applications/VCV Rack 2 Pro.app}
: ${VCVMON_USER_DIR:=${RACK_USER_DIR:-$HOME/Library/Application Support/Rack2}}

# full-width 3-line colored banner so state is visible through Rack's log spew
# usage: _vcvmon_banner <ansi-colors> <message>
_vcvmon_banner () {
	local colors=$1; shift
	local msg="$*"
	local cols=${COLUMNS:-80}
	local pad=$(( (cols - ${#msg}) / 2 ))
	(( pad < 0 )) && pad=0
	local blank line
	printf -v blank '%*s' $cols ''
	printf -v line '%*s%s' $pad '' "$msg"
	printf -v line '%-*s' $cols "$line"
	print -n -- "\e[${colors}m"
	print -r -- "$blank"
	print -r -- "$line"
	print -r -- "$blank"
	print -n -- "\e[0m"
}

_vcvmon_stamp () {
	find "$VCVMON_REPO" \
		\( -path "$VCVMON_REPO/.git" -o \
		   -path "$VCVMON_REPO/build" -o \
		   -path "$VCVMON_REPO/dist" -o \
		   -path "$VCVMON_REPO/scripts" \) -prune -o \
		\( -name '*.cpp' -o -name '*.hpp' -o -name '*.svg' -o -name 'plugin.json' -o -name 'Makefile' \) -print0 \
		| xargs -0 stat -f '%m %z %N' 2>/dev/null \
		| cksum
}

_vcvmon_kill_tree () {
	local parent=$1
	local child
	for child in ${(f)"$(pgrep -P "$parent" 2>/dev/null)"}; do
		_vcvmon_kill_tree "$child"
	done
	kill "$parent" 2>/dev/null
}

vcvmon () {
	emulate -L zsh
	unsetopt BG_NICE
	local mode=${1:-dev}
	if (( $# > 1 )) || [[ "$mode" != dev && "$mode" != dist ]]; then
		print -u2 "Usage: vcvmon [dev|dist]"
		return 1
	fi
	if [[ -z "$VCVMON_REPO" ]]; then
		VCVMON_REPO=$PWD
	fi
	if [[ ! -f "$VCVMON_REPO/plugin.json" ]]; then
		print -u2 "vcvmon: set VCVMON_REPO or run vcvmon from the plugin repo root"
		return 1
	fi
	if [[ "$mode" == dev && ! -d "$VCVMON_RACK" ]]; then
		print -u2 "vcvmon: set VCVMON_RACK to your Rack source directory"
		return 1
	fi
	if [[ "$mode" == dist ]]; then
		if [[ ! -f "$VCVMON_SDK/plugin.mk" || ! -x "$VCVMON_APP/Contents/MacOS/Rack" ]]; then
			print -u2 "vcvmon: dist needs VCVMON_SDK and VCVMON_APP pointing to the SDK and installed Rack app"
			return 1
		fi
	fi
	rm -f "$VCVMON_PAUSE"

	local key runner_pid=0 stamp next_stamp dirty=0
	local last_scan=0 now=0

	_vcvmon_dist_pids () {
		local pid executable
		while read -r pid executable; do
			[[ "$executable" == "$VCVMON_APP/Contents/MacOS/Rack" ]] && print -r -- "$pid"
		done < <(ps -ax -o pid= -o comm=)
		return 0
	}

	_vcvmon_close_dist () {
		local pids=$(_vcvmon_dist_pids) attempt
		[[ -z "$pids" ]] && return 0
		# Quit through the app so Rack can save its session. Never force-kill
		# an installed Rack that is waiting for a dialog.
		/usr/bin/osascript -e 'on run argv' \
			-e 'with timeout of 10 seconds' \
			-e 'tell application (item 1 of argv) to quit' \
			-e 'end timeout' -e 'end run' \
			"$VCVMON_APP" || return 1
		for attempt in {1..50}; do
			[[ -z "$(_vcvmon_dist_pids)" ]] && return 0
			sleep 0.2
		done
		print -u2 "vcvmon: Rack has not quit; close its dialog and press r to retry"
		return 1
	}

	_vcvmon_start_rack () {
		if [[ "$mode" == dist ]]; then
			(cd "$VCVMON_APP/Contents/MacOS" && RACK_USER_DIR="$VCVMON_USER_DIR" ./Rack) &!
			runner_pid=$!
			return
		fi
		(
			cd "$VCVMON_RACK" || exit 1
			make plugins run
		) &!
		runner_pid=$!
	}

	_vcvmon_stop_rack () {
		if [[ "$mode" == dist ]]; then
			(( runner_pid > 0 )) && _vcvmon_close_dist
			runner_pid=0
			return
		fi
		if (( runner_pid > 0 )) && kill -0 "$runner_pid" 2>/dev/null; then
			_vcvmon_kill_tree "$runner_pid"
			wait "$runner_pid" 2>/dev/null
		fi
		runner_pid=0
	}

	_vcvmon_restart_rack () {
		local message="$*" build_stamp=$(_vcvmon_stamp)
		[[ -n "$message" ]] && _vcvmon_banner "1;30;42" "$message"
		if [[ "$mode" == dist ]]; then
			local arch package plugins_dir
			case $(uname -m) in
				arm64) arch=arm64 ;;
				x86_64) arch=x64 ;;
				*) print -u2 "vcvmon: unsupported Mac architecture"; return 1 ;;
			esac
			if ! (cd "$VCVMON_REPO" && make RACK_DIR="$VCVMON_SDK" dist); then
				_vcvmon_banner "1;97;41" "VCVMON BUILD FAILED - Rack left running   [r] retry"
				return 1
			fi
			local packages=("$VCVMON_REPO"/dist/*-mac-$arch.vcvplugin(N))
			if (( ${#packages} != 1 )); then
				print -u2 "vcvmon: expected one dist package for mac-$arch"
				return 1
			fi
			package=$packages[1]
			_vcvmon_close_dist || return 1
			runner_pid=0
			plugins_dir="$VCVMON_USER_DIR/plugins-mac-$arch"
			if ! mkdir -p "$plugins_dir" || ! cp -f "$package" "$plugins_dir/"; then
				print -u2 "vcvmon: install failed"
				_vcvmon_start_rack
				return 1
			fi
		else
			_vcvmon_stop_rack
		fi
		_vcvmon_start_rack
		stamp=$build_stamp
		dirty=0
	}

	_vcvmon_banner "1;30;42" "VCVMON $VCVMON_VERSION $mode RUNNING   [space] pause/resume   [r] rebuild   [q] quit"
	stamp=$(_vcvmon_stamp)
	if [[ "$mode" == dist ]]; then
		_vcvmon_restart_rack "VCVMON dist - building plugin + launching installed Rack"
	else
		_vcvmon_start_rack
	fi

	{
		while true; do
			if read -t 0.5 -sk1 key; then
				case $key in
					' ')
						if [[ -f "$VCVMON_PAUSE" ]]; then
							rm -f "$VCVMON_PAUSE"
							if (( dirty )); then
								_vcvmon_restart_rack "VCVMON RUNNING - rebuilding queued changes   [space] to pause"
							else
								_vcvmon_banner "1;30;42" "VCVMON RUNNING   [space] to pause"
							fi
						else
							touch "$VCVMON_PAUSE"
							_vcvmon_banner "1;30;43" "VCVMON PAUSED   [space] to resume + rebuild"
						fi
						;;
					r|R)
						rm -f "$VCVMON_PAUSE"
						_vcvmon_restart_rack "VCVMON REBUILD"
						;;
					q|Q)
						break
						;;
				esac
			fi

			now=$(date +%s)
			if (( now == last_scan )); then
				continue
			fi
			last_scan=$now
			next_stamp=$(_vcvmon_stamp)
			if [[ "$next_stamp" != "$stamp" ]]; then
				stamp="$next_stamp"
				if [[ -f "$VCVMON_PAUSE" ]]; then
					if (( ! dirty )); then
						_vcvmon_banner "1;30;43" "VCVMON PAUSED - rebuild queued   [space] to resume + rebuild"
					fi
					dirty=1
				else
					_vcvmon_restart_rack "VCVMON CHANGE - rebuilding   [space] to pause"
				fi
			fi
		done
	} always {
		_vcvmon_stop_rack
		rm -f "$VCVMON_PAUSE"
		_vcvmon_banner "1;97;41" "VCVMON STOPPED"
	}
}

# still usable from other terminals (or by an agent)
alias vcvpause='touch $VCVMON_PAUSE; echo "vcvmon paused"'
alias vcvresume='rm -f $VCVMON_PAUSE; echo "vcvmon resumed"'
