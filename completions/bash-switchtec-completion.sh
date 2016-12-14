# bash tab completion for the switch command line utility
# Based on file from nvme-cli:
#   Kelly Kaoudis kelly.n.kaoudis at intel.com, Aug. 2015

_cmds="list test hard-reset fw-update fw-img-info version help"

_switchtec_list_opts () {
	local opts=""
	local compargs=""

	local nonopt_args=0
	for (( i=0; i < ${#words[@]}-1; i++ )); do
		if [[ ${words[i]} != -* ]]; then
			let nonopt_args+=1
		fi
	done

	if [ $nonopt_args -eq 2 ]; then
		opts="/dev/switchtec* "
	fi

	case "$1" in
		"hard-reset")
			opts+=" -y"
			;;
		"fw-update")
			if [ $nonopt_args -eq 3 ]; then
				compargs="-o filenames -A file"
			fi
			opts+=" -y"
			;;
		"fw-img-info")
			opts=""
			if [ $nonopt_args -eq 2 ]; then
				compargs="-o filenames -A file"
			fi
			;;
		"list"|"version")
			opts=""
			;;
		"help")
			opts=$_cmds
			;;
	esac

	opts+=" -h --help"

	COMPREPLY+=( $( compgen $compargs -W "$opts" -- $cur ) )

	return 0
}

_switchtec_subcmds () {
        local cur prev words cword
	_init_completion || return

	if [[ ${#words[*]} -lt 3 ]]; then
		COMPREPLY+=( $(compgen -W "$_cmds" -- $cur ) )
	else
		_switchtec_list_opts ${words[1]}
	fi

	return 0
}

complete -F _switchtec_subcmds switchtec
complete -F _switchtec_subcmds ./switchtec
