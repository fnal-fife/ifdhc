
SRCDIR=../../ifdh/
VPATH=$(SRCDIR)

#
# Note: set PYTHON=python3 and PYMAJOR=3 to build a system python3 version
#       set PYTHON_INCLUDE, PYTHON_LIB, and PYMAJOR to use non-system python.
#

PYVER:= $(shell $${PYTHON:-python} -V | sed -e 's/\..*//')
UNAME:= $(shell uname -s)
ifeq ($(PYVER),Python 3) 
SWIGOPTS=-py3
FALLBACK=fallback.python3
else
SWIGOPTS=
FALLBACK=fallback
endif
ifeq ($(UNAME),Darwin)
SHLIB=dylib
else
SHLIB=so
endif
LIB=libifdh.$(SHLIB)
HDR=ifdh.h
SWIGCXXFLAGS=-fPIC -I../util -I../numsg -I../../util -I../../numsg -I../../ifdh -g -Dregister= -I$${PYTHON_INCLUDE:-`echo $${VIRTUAL_ENV:-/usr}/include/python$${PYMAJOR:-}*`} $(ARCH)
CXXFLAGS=-pedantic-errors -Wall -Wextra -Werror -fPIC -I../../util -I../util -I ../../numsg -I../numsg -g  $(ARCH) -I$(SRCDIR) -std=c++11
CFLAGS=-pedantic-errors -Wall -Wextra -Werror -fPIC -I../../util -I../util -I ../../numsg -I../numsg -g  $(ARCH) -I$(SRCDIR) -std=c99
OBJ=ifdh.o ifdh_cp.o
ALL_OBJS=$(OBJ) ../util/WebAPI.o ../numsg/numsg.o ../util/SyslogAPI.o ../util/utils.o ../util/regwrap.o ../util/Checksum.o ../util/md5.o ../util/WimpyConfigParser.o ../util/sha256.o
genOBJ=ifdh_main.o ifdh_pybindgen.o python/ifdh.$(SHLIB)
genSRC=ifdh_main.cc
#genSRC=ifdh_pybindgen.cc ifdh_main.cc
PYMOD_OBJS=$(ALL_OBJS) ifdh_pybindgen.o
ifeq ($(UNAME),Darwin)
LDLIBS=-lz -lcrypto
else
LDLIBS=-lz -luuid -lcrypto
endif
DIRS=python
ALL=ifdh python/ifdh.$(SHLIB) libifdh.$(SHLIB) libifdh.a

all: $(DIRS) $(ALL) 

install:
	test -d ../lib/python || mkdir -p ../lib/python
	test -d ../bin || mkdir ../bin
	cp ifdh   ../bin
	cp $(LIB) ../lib
	cp $(HDR) ../include
	cp python/ifdh.$(SHLIB) ../lib/python
	#cp python/ifdh.py ../lib/python
	cp www_cp.sh xrdwrap.sh ../bin

clean: 
	rm -f $(OBJ) $(genOBJ) $(genSRC) $(LIB) $(ALL)

python:
	mkdir python


ifdh_main.cc: h_to_main.sh ifdh.h
	sh $^ > $@

ifdh: libifdh.$(SHLIB) ifdh_main.o 
	${CXX} $(ARCH) -o $@ ifdh_main.o $(ALL_OBJS) $(LDLIBS)
	./ifdh --help | perl -pe 's{\033\[.*?m}{}g; s{\tifdh ([^ ]*) *(.*?) *$$}{* *ifdh* *$$1* _$$2_}; s{\t--}{** }' > ifdh.textile

# at the moment, you need a custom pybindgen, so I am leaving this rule
# out and pretending ifdh_pybindgen.cc is just plain generated source.
# ifdh_pybindgen.cc: ifdh_pybindgen.py
#	python $< > $@
#	patch < ifdh_pybindgen.patch

ifdh_pybindgen.o: ifdh_pybindgen.cc
	${CXX} -c -o $@  $(SWIGCXXFLAGS) $< 

python/ifdh.$(SHLIB): $(PYMOD_OBJS)
	${CXX} --shared $(ARCH) -o  $@ $(PYMOD_OBJS) `ls $${PYTHON_LIB:-/usr/lib64}/libpython$${PYMAJOR:-}*[asd]* | head -1` $(LDLIBS) || ${CXX} --shared $(ARCH) -o  $@ $(PYMOD_OBJS)  `ls $${PYTHON_LIB:-/usr/lib}/libpython$${PYMAJOR:-}*[asd]* | head -1` $(LDLIBS)

#python/_ifdh.$(SHLIB): $(PYMOD_OBJS)
#	test -d python || mkdir python 
#	${CXX} --shared $(ARCH) -o  $@ $(PYMOD_OBJS) $${PYTHON_LIB:-/usr/lib64}/libpython*[asd]* $(LDLIBS) || ${CXX} --shared $(ARCH) -o  $@ $(PYMOD_OBJS) $${PYTHON_LIB:-/usr/lib}/libpython*[asd]* $(LDLIBS)

libifdh.$(SHLIB): $(ALL_OBJS)
	${CXX} --shared $(ARCH) -Wl,-soname,libifdh.$(SHLIB) $(CPPFLAGS)  -o $@ $(ALL_OBJS) $(LDLIBS)

libifdh.a: $(ALL_OBJS)
	rm -f $@
	ar qv $@ $(ALL_OBJS)
	ranlib $@ 

ifdh.o: ifdh.h

ifdh.o: ifdh.cc 
	${CXX} $(CXXFLAGS) -c -o $@ $<

ifdh_main.o: ifdh_main.cc
	${CXX} $(CXXFLAGS) -c -o $@ ifdh_main.cc

%.o: %.cc
	${CXX} -c -o $@  $(CXXFLAGS) $< 

ifdh_cp.o: ifdh_cp.cc ifdh.h ../util/WebAPI.h ../util/../numsg/numsg.h ../util/../numsg/../util/SyslogAPI.h ../util/regwrap.h

ifdh_main.o: ifdh_main.cc ifdh.h ../util/WebAPI.h 

ifdh.o: ifdh.cc ifdh.h ../util/WebAPI.h ../util/../numsg/numsg.h ../util/../numsg/../util/SyslogAPI.h ifdh.h ../util/WebAPI.h  ../util/sha256.h
