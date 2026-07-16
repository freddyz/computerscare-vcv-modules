# vcvmon - watch this repo, rebuild + run Rack on changes, with keyboard pause/resume.
# Source from ~/.zshrc:  source /Users/adammalone/dev/computerscare-vcv-modules/scripts/vcvmon.zsh
# Paths are machine-specific to this machine.

VCVMON_PAUSE=/tmp/vcvmon.pause
VCVMON_REPO=/Users/adammalone/dev/computerscare-vcv-modules
VCVMON_RACK=/Users/adammalone/dev/VCV-Rack/Rack
VCVMON_VERSION="polling-v2"

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
		   -path "$VCVMON_REPO/scripts" \) -prune -o \
		\( -name '*.cpp' -o -name '*.hpp' -o -name '*.svg' \) -print0 \
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
	rm -f "$VCVMON_PAUSE"

	local key runner_pid=0 stamp next_stamp dirty=0
	local last_scan=0 now=0

	_vcvmon_start_rack () {
		(
			cd "$VCVMON_RACK" || exit 1
			make plugins run
		) &!
		runner_pid=$!
	}

	_vcvmon_stop_rack () {
		if (( runner_pid > 0 )) && kill -0 "$runner_pid" 2>/dev/null; then
			_vcvmon_kill_tree "$runner_pid"
			wait "$runner_pid" 2>/dev/null
		fi
		runner_pid=0
	}

	_vcvmon_restart_rack () {
		local message="$*"
		[[ -n "$message" ]] && _vcvmon_banner "1;30;42" "$message"
		_vcvmon_stop_rack
		_vcvmon_start_rack
		stamp=$(_vcvmon_stamp)
		dirty=0
	}

	_vcvmon_banner "1;30;42" "VCVMON $VCVMON_VERSION RUNNING   [space] pause/resume   [r] rebuild   [q] quit"
	stamp=$(_vcvmon_stamp)
	_vcvmon_start_rack

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
