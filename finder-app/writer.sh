#!/bin/sh

set -e
set -u

if [ $# -eq 2 ]
then	
	if [ -f "$1" ] || [ -z "$2" ]
	then
		exit 1
	fi

	writefile=$1
	writestr=$2
else
	echo "bad argument"
	exit 1
fi

dirname "$writefile" | sudo xargs mkdir -p
echo "$writestr" >> "$writefile"

