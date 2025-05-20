#!/bin/bash

while [ $# -gt 1 ]
do
case x$1 in
x--experiment) export EXPERIMENT=$2; shift; shift;;
x*)            echo "unknown argument $1"; shift;;
esac
done
export watchpid=$PPID

# get initial token 

export IFDH_TOKEN_ENABLE=1

ifdh getToken

# check/refresh every 15 minutes in background until
# our parent process exits
(
   while kill -0 $watchpid
   do
      # check every 15 minutes
      sleep 900
      ifdh getToken 
   done
) > /dev/null &

exit 0
