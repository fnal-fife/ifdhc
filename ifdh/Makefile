SRCDIR=../../ifdh/
VPATH=$(SRCDIR)

LIB=libifdh.so
HDR=ifdh.h
SWIGCXXFLAGS=-fPIC -I../util -I../numsg -I../../util -I../../numsg -I../../ifdh -g -I`echo $${PYTHON_INCLUDE:-$${PYTHON_DIR:-/usr}/include/python*}` $(ARCH)
CXXFLAGS=-pedantic-errors -Wall -Wextra -Werror -fPIC -I../../util -I../util -I ../../numsg -I../numsg -g  $(ARCH) -I$(SRCDIR)
OBJ=ifdh.o ifdh_cp.o
ALL_OBJS=$(OBJ) ../util/WebAPI.o ../numsg/numsg.o ../util/SyslogAPI.o ../util/utils.o ../util/regwrap.o
genOBJ=ifdh_main.o ifdh_wrap.o python/_ifdh.so
genSRC=ifdh_wrap.cxx ifdh_main.cc
PYMOD_OBJS=$(ALL_OBJS) ifdh_wrap.o


all: ifdh python python/_ifdh.so libifdh.so libifdh.a

install:
	test -d ../lib/python || mkdir -p ../lib/python
	test -d ../bin || mkdir ../bin
	cp ifdh   ../bin
	cp $(LIB) ../lib
	cp $(HDR) ../include
	cp python/* ../lib/python

clean: 
	rm -f $(OBJ) $(genOBJ) $(genSRC) $(LIB) libifdh.a

python:
	mkdir python


ifdh_main.cc: h_to_main.sh ifdh.h
	sh $^ > $@

ifdh: libifdh.so ifdh_main.o 
	g++ $(ARCH) -o $@ ifdh_main.o $(ALL_OBJS)
	./ifdh --help | perl -pe 's{\033\[.*?m}{}g; s{\tifdh ([^ ]*) *(.*?) *$$}{* *ifdh* *$$1* _$$2_}; s{\t--}{** }' > ifdh.textile

python/_ifdh.so: $(PYMOD_OBJS)
	test -d python || mkdir python 
	g++ --shared $(ARCH) -o  $@ $(PYMOD_OBJS) $${PYTHON_LIB:-$${PYTHON_DIR:-/usr}/lib64}/python*/config/libpython*.[as]* || g++ --shared $(ARCH) -o  $@ $(PYMOD_OBJS) $${PYTHON_LIB:-$${PYTHON_DIR:-/usr}/lib}/python*/config/libpython*.[as]*


libifdh.so: $(ALL_OBJS)
	g++ --shared $(ARCH) $(CPPFLAGS)  -o $@ $(ALL_OBJS)

libifdh.a: $(ALL_OBJS)
	rm -f $@
	ar qv $@ $(ALL_OBJS)
	ranlib $@ 

ifdh_wrap.cxx: ifdh.h 

ifdh_wrap.cxx: ifdh.i 
	test -d python || mkdir python
	swig -python -c++ -o ifdh_wrap.cxx $<
	set -x ; python -V 2>&1 | grep ' 2.[67]' > /dev/null && rpm -qf /usr/bin/swig | grep 1.3.[42][90] && (patch -p0 < ifdh_wrap.patch ||  patch -p0 < ../../ifdh/ifdh_wrap.patch) || true
	mv ifdh.py python/ifdh.py

ifdh_wrap.o: ifdh_wrap.cxx
	set -x ; g++ $(SWIGCXXFLAGS) $(ARCH) -c -o $@ $<
        
ifdh.o: ifdh.h

ifdh.o: ifdh.cc 
	g++ $(CXXFLAGS) -c -o $@ $<

ifdh_main.o: ifdh_main.cc
	g++ $(CXXFLAGS) -c -o $@ ifdh_main.cc

%.o: %.cc
	g++ -c -o $@  $(CXXFLAGS) $< 

ifdh_cp.o: ifdh_cp.cc ifdh.h ../util/WebAPI.h ../util/../numsg/numsg.h ../util/../numsg/../util/SyslogAPI.h 

ifdh_main.o: ifdh_main.cc ifdh.h ../util/WebAPI.h 

ifdh.o: ifdh.cc ifdh.h ../util/WebAPI.h ../util/../numsg/numsg.h ../util/../numsg/../util/SyslogAPI.h ifdh.h ../util/WebAPI.h 
