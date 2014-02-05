#!/bin/sh

#set -x


#
# IFDH_STAGE_VIA is allowed to be a literal '$OSG_SITE_WRITE'
# (or generally $E for some environment variable E)
# so we eval it to expand that
#

debug() {
    [ x$IFDH_DEBUG != x ] && echo "$@"
}

run_with_timeout() {
    timeout=$1
    poll=5
    shift
    "$@" &
    cpid=$!
    while kill -0 $cpid 2>/dev/null&& [ $timeout -gt 0 ]
    do
        sleep $poll
        timeout=$((timeout - poll))
    done
    if kill -0 $cpid 2>/dev/null
    then
         echo "killing: $1 due to timeout"
         kill -9 $cpid
    fi
    wait $cpid
}

init() {

    echo "Copyback v1_2_5 starting"
    for cmd in "ifdh --help" "lcg-cp -help" "srmls -help"
    do
	if [ `$cmd 2>&1 | wc -l` -gt 2 ]
	then
	    : 
	else
	    echo "Cannot get output of $cmd; bailing out..."
	    exit 1
	fi
    done
    
 
    printf "ifdh copyback script starting for experiment %s\n" "${EXPERIMENT:-nova}"

    IFDH_STAGE_VIA="${IFDH_STAGE_VIA:-$OSG_SITE_WRITE}"
    eval IFDH_STAGE_VIA=\""${IFDH_STAGE_VIA}"\"

    case "$IFDH_STAGE_VIA" in
    *=\>*) 
        remap='case `hostname` in '`echo "$IFDH_STAGE_VIA" | sed -e 's/=>/)echo /g'`';; esac'
        IFDH_STAGE_VIA=`eval "$remap"`
    esac

    host=`hostname --fqdn`
    filelist=${TMPDIR:-/tmp}/filelist$$
    wprefix="${IFDH_STAGE_VIA}/${EXPERIMENT:-nova}/ifdh_stage"

    #
    # paths with double slashes confuse srmls...
    #
    while :
    do
      case "$wprefix" in
      *//*//*)  wprefix=`echo "$wprefix" | sed -e 's;\(//.*/\)/;\1;'`;;
      *)  break;;
      esac
    done

    ifdh log "ifdh_copyback.sh: starting for ${EXPERIMENT} on $host location $wprefix"
    # avoid falling into a hall of mirrors
    unset IFDH_STAGE_VIA

    cd ${TMPDIR:-/tmp}
}


get_first() {
   #debugging
   printf "Lock dir listing output:\n" >&2
   srmls -2 "$wprefix/lock" >&2
   printf "::\n" >&2

   srmls -2 "$wprefix/lock" | sed -e '1d' -e '/^$/d' | sort | head -1
}

i_am_first() {
   lockfile=`get_first` 
   debug "comparing $lockfile to $uniqfile" 
   case "$lockfile" in
   *$uniqfile) return 0;;
   *)            return 1;;
   esac
}

expired_lock() {
   set : `get_first`
   first_file="$3"
   if [ "x$first_file" != "x" ] && [ "`basename $first_file`" != "lock" ]
   then
       printf "Checking lock file: $first_file ... "
       # example timestamp:  t_2013-09-24_12_21_22_red-d21n13.red.hcc.unl.edu_30277
       first_time=`echo $first_file | sed -e 's;.*/t_\([0-9]*\).\([0-9]*\).\([0-9]*\).\([0-9]*\).\([0-9]*\).\([0-9]*\).*;\1-\2-\3 \4:\5:\6;'`
       first_secs=`date --date="$first_time" '+%s'`
       cur_secs=`date '+%s'`


       delta=$((cur_secs - first_secs))

       printf "lock is $delta seconds old: "

       # if the lock is over 4 hours old, we consider it dead

       if [ $delta -gt 14400 ]
       then
	   lock_name=`basename $first_file`
	   printf "Breaking expired lock.\n"
           ifdh log "ifdh_copyback.sh: Breaking expired lock $lock_name\n"
	   srmrm $wprefix/lock/$lock_name
       else
           printf "Still valid.\n"
       fi
   fi
}

get_lock() {

   expired_lock

   datestamp=`date +%FT%T | sed -e 's/[^a-z0-9]/_/g'`
   uniqfile="t_${datestamp}_${host}_$$"

   if [ "x$datestamp" = x  -o "x$host" = x ]
   then
      # lock algorithm is busted.. bail
      return 1
   fi

   echo lock > ${TMPDIR:-/tmp}/$uniqfile
   run_with_timeout 300 ifdh cp ${TMPDIR:-/tmp}/$uniqfile $wprefix/lock/$uniqfile
   if i_am_first
   then
      sleep 5
      if i_am_first
      then
         echo "Obtained lock $uniqfile at `date`"
         ifdh log "ifdh_copyback.sh: Obtained lock $uniqfile at `date`"
         return 0  
      fi
   fi

   printf "Lock $lockfile already exists, someone else is already copying files.\n"
   srmrm $wprefix/lock/$uniqfile
   return 1
}

clean_lock() {
   printf "Removing my lock: $wprefix/lock/$uniqfile\n"
   srmrm $wprefix/lock/$uniqfile > /dev/null 2>&1
}

have_queue() {
   count=`srmls -2  $wprefix/queue | wc -l`
   [ $count -gt 2 ]
}

copy_files() {
   srmls -2 $wprefix/queue | (
     read dirname
     while read size filename
     do

         [ x$filename = x ] && continue

         rm -f ${filelist}
         filename=`basename $filename`

         printf "Fetching queue entry: $filename\n"
         run_with_timeout 300  ifdh cp $wprefix/queue/$filename ${filelist}

         #printf "queue entry contents:\n-------------\n"
         #cat ${filelist}
         #printf "\n-------------\n"

         printf "starting copy..."
         ifdh log "ifdh_copyback.sh: starting copies for $filename"

         #
         #
         
         while read src dest
         do              
  	     # fixup plain fermi destinations
             case "$dest" in
             srm:*) ;;
             /*)  dest="srm://fg-bestman1.fnal.gov:10443/srm/v2/server?SFN=$dest"
;;
             esac

             cmd="ifdh cp  \"$src\" \"$dest\""
             echo "ifdh_copyback.sh: $cmd"
             ifdh log "ifdh_copyback.sh: $cmd"
             run_with_timeout 3600 eval "$cmd"
             debug "status; $?"

             if  srmls -2 "$dest" > /dev/null 2>&1
             then
                 :
             else
                 msg="Not cleaning queue entry $filename, because destination $dest would not list"
                 echo "$msg"
                 ifdh log "$msg"
                 break 2
             fi
         done < $filelist

         printf "completed.\n"
         ifdh log "ifdh_copyback.sh: finished copies for $filename"

         printf "Cleaning up for $filename:\n"
         while read src dest
         do
             printf "removing: $src\n"
             ifdh log "ifdh_copyback.sh: cleaned up $src"

             srmrm $src &
             srcdir=`dirname $src`
         done < ${filelist}

         wait

         printf "removing: $srcdir\n"
         srmrmdir $srcdir &

         printf "removing: $wprefix/queue/$filename\n"
         srmrm    $wprefix/queue/$filename &
         ifdh log "ifdh_copyback.sh: cleaned up $filename"

         rm $filelist

     done
  )
}

debug_ls() {
   debug "current stage area:"
   [ x$IFDH_DEBUG != x ] && srmls -2 --recursion_depth=4 $wprefix
   debug ""
}

copy_daemon() {
   init

   debug_ls

   trap "clean_lock" 0 1 2 3 4 5 6 7 8 10 12

   while get_lock && have_queue
   do
       copy_files
       clean_lock
   done

   clean_lock
   
   printf "Finished on $host at `date`\n"

   ifdh log "ifdh_copyback.sh: finished on $host"

   trap "" 0 1 2 3 4 5 6 7 8 10 12
}

copy_daemon
