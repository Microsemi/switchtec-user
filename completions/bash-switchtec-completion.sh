# bash tab completion for the switch command line utility
# Based on file from nvme-cli:
#   Kelly Kaoudis kelly.n.kaoudis at intel.com, Aug. 2015

_cmds="list test hard-reset help"

_switchtec_list_opts () {
	local opts=""

	local device
	for (( i=0; i < ${#words[@]}-1; i++ )); do
		if [[ ${words[i]} == /dev/switchtec* ]]; then
		    device=${words[i]}
		fi
	done

	if [[ -z $device ]]; then
	    opts="/dev/switchtec*"
	fi

	case "$1" in
		"list")
		opts=" "
			;;
		"help")
		opts=$_cmds
			;;
	esac

	COMPREPLY+=( $( compgen -W "$opts" -- $cur ) )

	return 0
}

_switchtec_subcmds () {
        local cur prev words cword
	_init_completion || return

	if [[ ${#words[*]} -lt 3 ]]; then
		COMPREPLY+=( $(compgen -W "$_cmds" -- $cur ) )
	else
		_switchtec_list_opts $prev
	fi

	return 0
}

complete -F _switchtec_subcmds switchtec
complete -F _switchtec_subcmds ./switchtec
