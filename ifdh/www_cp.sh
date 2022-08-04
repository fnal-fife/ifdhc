#!/bin/bash


# "cp" style utility for web locations, uses curl or gfal...
IFDH_HTTP_CMD="${IFDH_HTTP_CMD:-gfal}"

curlopts="-f -L --silent --capath ${X509_CERT_DIR:=/etc/grid-security/certificates} "
gfalopts="-t 14400"
case "x$IFDH_DEBUG" in
x1)
    curlopts="$curlopts -v"
    gfalopts="$gfalopts -v"
    ;;
x2)
    set -x
    echo X509_USER_PROXY $X509_USER_PROXY >&2
    echo BEARER_TOKEN_FILE $BEARER_TOKEN_FILE >&2
    curlopts="$curlopts -vvv"
    gfalopts="$gfalopts -vvv"
    ;;
esac

# allow extra curl or gfal flags. Harold, your abstraction is leaking...

while :
do
case "x$1" in
x--ll*|x--ls*|x--mv*|x--rmdir*|x--mkdir*|x--chmod*) break;;
x-*)  curlopts="$curlopts $1"; gfalopts="$gfalopts $1"; shift;;
x*)   break;;
esac
done

src="$1"
dst="$2"


#
# fun with scitokens...
#   also do tokens OR proxies, but not both.  Tokens win if present
#
if [[ "$src$dst" =~ dbdata[a-z0-9]*\.fnal\.gov ]]> /dev/null
then
   curlopts="$curlopts $IFDH_UCONDB_OPTS "
   IFDH_HTTP_CMD="curl"
elif [ "x${BEARER_TOKEN}" != "x" ]
then
    curlopts="$curlopts -H 'Authorization: Bearer ${BEARER_TOKEN}'"
elif [ -r "${BEARER_TOKEN_FILE:=$XDG_RUNTIME_DIR/bt_u`id -u`}" ]
then
    curlopts="$curlopts -H 'Authorization: Bearer `cat ${BEARER_TOKEN_FILE}`'"
    # gfal2 only looks at BEARER_TOKEN contents currently. See https://its.cern.ch/jira/browse/DMC-1226
    export BEARER_TOKEN=`cat ${BEARER_TOKEN_FILE}`
elif [ -r "${X509_USER_PROXY:=/tmp/x509up_u`id -u`}" ]
then
    curlopts="$curlopts --cert $X509_USER_PROXY --key $X509_USER_PROXY --cacert $X509_USER_PROXY "
fi


if [ x$IFDH_UCONDB_UPASS != x ]
then
   curlopts="$curlopts --digest -u $IFDH_UCONDB_UPASS "
fi

#
# convert ucondb urls into real urls
#
ucondb_loc="http://dbdata0vm.fnal.gov:8201"
ucondb_convert() {
   echo $1 | 
       sed -e "s;^ucondb:/*\\(.*\\)/\\(.*\\)/\\(.*\\);$ucondb_loc/\\1_ucon_prod/app/data/\\2/\\3;"
}

wmkdir() {
   eval "curl $curlopts -o - -X MKCOL '$1'"
   if [ $? = 22 ]
   then
       echo "error: File exists" >&2
   fi
}

wrm() {
   eval "curl $curlopts -o - -X DELETE '$1'"
}

wmv() {
   eval "curl $curlopts -X MOVE --header 'Destination:$2' '$1'"
}

wll() {
    eval "curl $curlopts -o -  -H 'Depth: 1' -X PROPFIND '$1'" <<EOF  
<?xml version="1.0"?>
<a:propfind xmlns:a="DAV:">
 <a:allprop/>
</a:propfind>
EOF
}

wls() {
  wll "$1" >/dev/null && wll "$1"  | perl -pe 's;</;\n</;go; s;><;>\n<;go;' | egrep '<d:href>|<d:getcontentlength' | perl -pe 'if(/<d:href>/){ chomp();} s{<d:getcontentlength/>}{ 0}o; s{<[^>]*>}{ }go;'
}


dst=`ucondb_convert "$dst"`
src=`ucondb_convert "$src"`

#echo "$src;$dst"

do_curl() {
   curlopts="$1"
   src="$2"
   dst="$3"
   case "$src;$dst" in
   http*//*\;/*)
       eval "curl $curlopts -o '$dst' '$src'"
       ;;
   /*\;http*://*)
       eval "curl $curlopts -T '$src' '$dst'"
       ;;
   http*://*\;http*://*)
       eval "curl $curlopts -o - '$src'" | eval "curl $curlopts  -T - '$dst'"
       ;;

   --ls*)
       wls "$dst"
       ;;

   --ll*)
       wll "$dst"
       ;;

   --mkdir*)
       wmkdir "$dst"
       ;;

   --rmdir*|--rm*)
       wrm "$dst"
       ;;

   --mv*)
       wmv "$src" "$dst"
       ;;

   --ls*|--mv*|--rmdir*|--mkdir*|--chmod*)
       echo "Not yet implemented" >&2
       exit 1
       ;;
   esac
}

do_gfal() {
   gfalopts="$1"
   src="$2"
   dst="$3"
   unset PYTHONHOME; unset PYTHONPATH; unset LD_LIBRARY_PATH; unset GFAL_PLUGIN_DIR;
   PATH=/usr/bin
   if [ -r "${BEARER_TOKEN_FILE}" ]
   then
      export BEARER_TOKEN=`cat ${BEARER_TOKEN_FILE}`
   fi
   case "$src" in
   --ls*) eval "gfal-ls $gfalopts '$dst'";;
   --ll*) eval "gfal-ls -l $gfalopts '$dst'";;
   --mkdir*) eval "gfal-mkdir $gfalopts '$dst'";;
   --rmdir*) eval "gfal-rm -r $gfalopts '$dst'";;
   --rm*) eval "gfal-rm $gfalopts '$dst'";;
   --mv*) eval "gfal-rename $gfalopts '$src' '$dst'";;
   --chmod*)
       echo "Not yet implemented" >&2
       exit 1
       ;;
   *)
       eval "gfal-copy -f --checksum adler32 $gfalopts '$src' '$dst'";;
   esac
}

case "x$IFDH_HTTP_CMD" in
xcurl) do_curl "$curlopts" "$src" "$dst";;
xgfal) do_gfal "$gfalopts" "$src" "$dst";;
x*)    echo "invalid IFDH_HTTP_CMD: $IFDH_HTTP_CMD" >&2; exit 1;;
esac
