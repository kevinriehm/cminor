#!/usr/bin/bash

declare -A passes
declare -A totals

declare -A mode_param=(
	[resolver]=resolve
	[scanner]=scan
	[typechecker]=typecheck
	[parser]=parse
)

for mode in resolver scanner typechecker parser
do
	for type in good bad
	do
		key="$mode $type"
		passes[$key]=0
		totals[$key]=0

		[ $type == "good" ]; expected=$?

		for f in test/$mode/$type*.cminor
		do
			if [ ! -e $f ]
			then
				continue
			fi

			totals[$key]=$((totals[$key] + 1))

			output=`./cminor -${mode_param[$mode]} $f 2>&1`
			if [ $? -eq $expected ]
			then
				passes[$key]=$((passes[$key] + 1))
			else
				echo FAILED: $f
				echo $output
			fi
		done
	done
done

IFS=$'\n'
for key in `sort <<< "${!totals[*]}"`
do
	echo -e $key:\\t ${passes[$key]}/${totals[$key]}
done

