# vcvmon — watch this repo, rebuild + run Rack on changes, with keyboard pause/resume.
# Source from ~/.zshrc:  source /Users/adammalone/dev/computerscare-vcv-modules/scripts/vcvmon.zsh
# Paths are machine-specific to this machine.

VCVMON_PAUSE=/tmp/vcvmon.pause
VCVMON_REPO=/Users/adammalone/dev/computerscare-vcv-modules
VCVMON_RACK=/Users/adammalone/dev/VCV-Rack/Rack

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

vcvmon () {
	emulate -L zsh
	rm -f "$VCVMON_PAUSE"
	local fifo="/tmp/vcvmon.fifo.$$" fd pid key
	mkfifo "$fifo" || return 1
	# read-write fd keeps the FIFO open so nodemon's stdin never hits EOF,
	# and lets us send it "rs" to force a restart
	exec {fd}<>"$fifo"

	_vcvmon_banner "1;30;42" "VCVMON RUNNING   [space] pause/resume   [r] rebuild   [q] quit"

	nodemon \
		--watch "$VCVMON_REPO" \
		--ignore "$VCVMON_REPO/scripts/**" \
		-e cpp,hpp,svg \
		--exec "while [ -f $VCVMON_PAUSE ]; do sleep 1; done; cd $VCVMON_RACK; make plugins run || exit 1;" \
		<&$fd &!
	pid=$!

	{
		while kill -0 $pid 2>/dev/null && read -sk1 key; do
			case $key in
				' ')
					if [[ -f $VCVMON_PAUSE ]]; then
						rm -f "$VCVMON_PAUSE"
						print -u$fd rs
						_vcvmon_banner "1;30;42" "VCVMON RUNNING — rebuilding   [space] to pause"
					else
						touch "$VCVMON_PAUSE"
						_vcvmon_banner "1;30;43" "VCVMON PAUSED   [space] to resume + rebuild"
					fi
					;;
				r|R)
					rm -f "$VCVMON_PAUSE"
					print -u$fd rs
					_vcvmon_banner "1;30;42" "VCVMON REBUILD"
					;;
				q|Q) break ;;
			esac
		done
	} always {
		kill $pid 2>/dev/null
		exec {fd}<&-
		rm -f "$fifo" "$VCVMON_PAUSE"
		_vcvmon_banner "1;97;41" "VCVMON STOPPED"
	}
}

# still usable from other terminals (or by an agent)
alias vcvpause='touch $VCVMON_PAUSE; echo "vcvmon paused"'
alias vcvresume='rm -f $VCVMON_PAUSE; echo "vcvmon resumed"'
