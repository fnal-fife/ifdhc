SRCDIR=../../ifdh/
VPATH=$(SRCDIR)

UNAME:= $(shell uname -s)
ifeq ($(UNAME),Darwin)
SHLIB=dylib
else
SHLIB=so
endif
LIB=libifdh.$(SHLIB)
HDR=ifdh.h
SWIGCXXFLAGS=-fPIC -I../util -I../numsg -I../../util -I../../numsg -I../../ifdh -g -Dregister= -I$${PYTHON_INCLUDE:-`echo $${VIRTUAL_ENV:-/usr}/include/python*`} $(ARCH)
CXXFLAGS=-pedantic-errors -Wall -Wextra -Werror -fPIC -I../../util -I../util -I ../../numsg -I../numsg -g  $(ARCH) -I$(SRCDIR)
OBJ=ifdh.o ifdh_cp.o
ALL_OBJS=$(OBJ) ../util/WebAPI.o ../numsg/numsg.o ../util/SyslogAPI.o ../util/utils.o ../util/regwrap.o ../util/Checksum.o ../util/WimpyConfigParser.o
genOBJ=ifdh_main.o ifdh_wrap.o python/_ifdh.$(SHLIB)
genSRC=ifdh_wrap.cxx ifdh_main.cc
PYMOD_OBJS=$(ALL_OBJS) ifdh_wrap.o
LDLIBS=-lz -luuid
DIRS=python
ALL=ifdh python/_ifdh.$(SHLIB) libifdh.$(SHLIB) libifdh.a

all: $(DIRS) $(ALL) 

install:
	test -d ../lib/python || mkdir -p ../lib/python
	test -d ../bin || mkdir ../bin
	cp ifdh   ../bin
	cp $(LIB) ../lib
	cp $(HDR) ../include
	cp python/_ifdh.$(SHLIB) ../lib/python
	cp python/ifdh.py ../lib/python
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

python/_ifdh.$(SHLIB): $(PYMOD_OBJS)
	test -d python || mkdir python 
	${CXX} --shared $(ARCH) -o  $@ $(PYMOD_OBJS) $${PYTHON_LIB:-/usr/lib64}/libpython*[asd]* $(LDLIBS) || ${CXX} --shared $(ARCH) -o  $@ $(PYMOD_OBJS) $${PYTHON_LIB:-/usr/lib}/libpython*[asd]* $(LDLIBS)


libifdh.$(SHLIB): $(ALL_OBJS)
	${CXX} --shared $(ARCH) $(CPPFLAGS)  -o $@ $(ALL_OBJS) $(LDLIBS)

libifdh.a: $(ALL_OBJS)
	rm -f $@
	ar qv $@ $(ALL_OBJS)
	ranlib $@ 

ifdh_wrap.cxx: ifdh.h 

ifdh_wrap.cxx: ifdh.i 
	test -d python || mkdir python
	swig $(SWIGOPTS) -python -c++ -o ifdh_wrap.cxx $< || (cp ifdh_wrap.cxx.fallback ifdh_wrap.cxx && cp ifdh.py.fallback ifdh.py)
	set -x ; python -V 2>&1 | grep ' 2.[67]' > /dev/null && rpm -qf /usr/bin/swig | grep 1.3.[42][90] && (patch -p0 < ifdh_wrap.patch ||  patch -p0 < ../../ifdh/ifdh_wrap.patch) || true
	mv ifdh.py python/ifdh.py

ifdh_wrap.o: ifdh_wrap.cxx
	set -x ; ${CXX} $(SWIGCXXFLAGS) $(ARCH) -c -o $@ $<
        
ifdh.o: ifdh.h

ifdh.o: ifdh.cc 
	${CXX} $(CXXFLAGS) -c -o $@ $<

ifdh_main.o: ifdh_main.cc
	${CXX} $(CXXFLAGS) -c -o $@ ifdh_main.cc

%.o: %.cc
	${CXX} -c -o $@  $(CXXFLAGS) $< 

ifdh_cp.o: ifdh_cp.cc ifdh.h ../util/WebAPI.h ../util/../numsg/numsg.h ../util/../numsg/../util/SyslogAPI.h ../util/regwrap.h

ifdh_main.o: ifdh_main.cc ifdh.h ../util/WebAPI.h 

ifdh.o: ifdh.cc ifdh.h ../util/WebAPI.h ../util/../numsg/numsg.h ../util/../numsg/../util/SyslogAPI.h ifdh.h ../util/WebAPI.h 
