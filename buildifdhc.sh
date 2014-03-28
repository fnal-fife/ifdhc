# buildifdhc.sh <opt|debug|prof> [e2|gcc47]
# default is gcc47


extraqual=${2}
basequal=${1}

fullqual=${basequal}:${extraqual}
qualdir=${extraqual}-${basequal}

if [ "x$basequal" = "x" -o "x$extraqual" = "x" ]
then
    echo "usage: $0 <debug|prof|opt> e2"
    exit 1
fi

# use the art externals python, too
. `ups setup python v2_7_5` || true
. `ups setup gcc v4_8_1` || true

case $basequal in
debug) ARCH="-std=c++11 -O0 -g";;
prof)  ARCH="-std=c++11 -g -p -pg";;
opt)   ARCH="-std=c++11 -O2";;
esac
export ARCH

export PYTHON_CONF=`echo $PYTHON_LIB/python*/config`

fqdir="`ups flavor -4`-$qualdir"
fqdir="`echo $fqdir | sed -e 's/[^A-Za-z0-9]/-/g'`" 

rm -rf build
mkdir build
mkdir $fqdir
for d in util numsg ifdh
do
   mkdir -p build/$d
   cp $d/Makefile build/$d/Makefile
done

(cd build && make -f ../Makefile DESTDIR=../$fqdir/ all install-libs)

make install-headers
