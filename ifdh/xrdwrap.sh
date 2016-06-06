#!/bin/sh

#
# command that splits xrootd urls and then runs
# xrdfs to perform useful commands
#
for i in "$@"
do
   #echo "looking at $i"
   case "$i" in
   root:*|xroot:*) 
      host=`echo "$i" | sed -e 's;[a-z]*://\([^/]*\)\(.*\);\1;'`
      path=`echo "$i" | sed -e 's;[a-z]*://\([^/]*\)\(.*\);\2;'`
      #echo saw url host $host path $path
      args="$args '$path'"
      ;;
   *)
      args="$args '$i'"
      ;;
   esac
done

args="'$host' $args"

#echo "running: /usr/bin/xrdfs $args"
eval /usr/bin/xrdfs $args
