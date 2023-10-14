#!/bin/bash

filesdir=""
searchstr=""
totalFiles=0
matchLines=0

if [ $# -lt 2 ]
then
	if [ $# -lt 1 ]
	then
		echo "Please specify a filesdir as argument 1"
	fi
	echo "Please specify a searchstr as arfument 2"

	exit 1
else
	filesdir=$1
	searchstr=$2
fi

if [ -d "${filesdir}" ]
then
	totalFiles=$(ls -Rl | grep "^-" | wc -l)
	matchLines=$(grep -r -0 ${searchstr} ${filesdir} | wc -l)
	echo "The number of files are ${totalFiles} and the number of matching lines are ${matchLines}"
else
	echo "${filesdir} is not a valid directory"
	exit 1
fi

