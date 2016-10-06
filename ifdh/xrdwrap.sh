#!/bin/sh

#set -x

#
# command that splits xrootd urls and then runs
# xrdfs to perform useful commands
#

lscheck=false

for i in "$@"
do
   #echo "looking at $i"
   case "$i" in
   ls)
      args="$args '$i'"
      lscheck=true
      ;;
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

#
# annoyingly xrdfs does not do ls of files, it gives
# an error, so if we're asked to ls something that is
# a file, we have to list the parent and pull the
# file we want out.  To tell we want to do that
# we have to stat the path...
# so:
#  if it is an ls command
#     make a stat command out of it
#     if the stat command yeilds an IsDir flag
#        run the ls on the dirname of the path
#        and grep out the path we want
#     otherwise
#        run as usual
#  otherwise
#     run as usual
#
if $lscheck
then
    statargs=`echo "$args" | sed -e 's/ls/stat/' -e "s/'-l'//"`
    if eval /usr/bin/xrdfs $statargs | grep 'IsDir' > /dev/null
    then
        eval /usr/bin/xrdfs $args 
    else
        dpath=`dirname $path`
        args=`echo $args | sed -e "s;$path;$dpath;"`
        eval "/usr/bin/xrdfs $args | grep $path"
    fi
else
    #echo "running: /usr/bin/xrdfs $args"
    eval /usr/bin/xrdfs $args 
fi

