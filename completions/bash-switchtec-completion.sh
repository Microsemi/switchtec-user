
_switchtec_comp () {
        local cur prev words cword
	_init_completion || return
	local compargs=""

	unset words[${#words[@]}-1]
	opts=$(SWITCHTEC_COMPLETE=1 ${words[*]})
	compfile=$?

	if [ $compfile -eq 2 ]; then
		compopt -o filenames
		compargs="-f"
	fi

	COMPREPLY=( $( compgen $compargs -W "$opts" -- $cur ) )

	return 0
}

complete -F _switchtec_comp switchtec
complete -F _switchtec_comp ./switchtec
