#!/usr/bin/bash

# generate token hashes for shpincter

if [ $# == 3 ]; then
   
    if [ -f $3 ]; then
    
        t_hash=$(echo -n $2 | sha256sum | sed 's/[ \t-]*$//')
        echo "$1:$t_hash" >> $3

    else

        echo "file not found"

    fi


else

    echo "usage: genhash <id> <token> <tokenfile>"

fi
