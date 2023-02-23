#!/bin/sh

set -e
set -u

if [ $# -eq 2 ]
then	
	if [ -d !"$1" ] || [ -z "$2" ]
	then
		echo "bad aregument"
		exit 1
	fi

	filesdir=$1
	searchstr=$2
else
	echo "bad argument"
	exit 1
fi

totalfiles=$(find "${filesdir}"/* | wc -l)
filematched=$(grep -r "${searchstr}" "${filesdir}"/* | wc -l)

echo "The number of files are $totalfiles and the number of matching lines are $filematched"

