#!/bin/bash

writefile=""
writestr=""

if [ $# -lt 2 ] 
then
        if [ $# -lt 1 ] 
        then
                echo "Please specify a writefile as argument 1"
        fi  
        echo "Please specify a writestr as arfument 2"

        exit 1
else
        writefile=$1
        writestr=$2
fi


$(mkdir -p "$(dirname "${writefile}")" && touch "${writefile}")

if [ -f ${writefile} ]
then
	$(echo "${writestr}" > ${writefile}) 
	exit 0
else
	echo "Failed to create file"
	exit 1
fi

