
SUBDIRS= util fife_wda numsg nucondb ifbeam ifdh

all: 
	for d in $(SUBDIRS); do ([ -d $$d ] && cd $$d && make $@); done

clean:
	for d in $(SUBDIRS); do ([ -d $$d ] && cd $$d && make $@); done
	rm -rf build_art 

test:
	cd tests && python TestSuite.py

install: install-headers install-libs

install-libs: all
	rm -rf $(DESTDIR)lib 
	test -d $(DESTDIR)lib || mkdir -p  $(DESTDIR)lib && cp [inu]*/*.so [inu]*/*.a  $(DESTDIR)lib
	test -d $(DESTDIR)lib/python || mkdir -p  $(DESTDIR)lib/python && cp ifdh/python/*  $(DESTDIR)lib/python
	[ -r ifdh/ifdh_fetch.py ] && cp ifdh/ifdh_fetch.py lib/python || cp ../ifdh/ifdh_fetch.py lib/python
	test -d $(DESTDIR)bin || mkdir -p $(DESTDIR)bin 	
	cp ifdh/ifdh $(DESTDIR)bin
	[ -r ifdh/ifdh_copyback.sh ] && cp ifdh/ifdh_copyback.sh ifdh/ifdh_fetch $(DESTDIR)bin || cp ../ifdh/ifdh_copyback.sh ../ifdh/ifdh_fetch $(DESTDIR)bin

install-headers:
	rm -rf $(DESTDIR)inc
	test -d $(DESTDIR)inc || mkdir -p $(DESTDIR)inc && cp [finu]*/*.h $(DESTDIR)inc

32bit:
	ARCH="-m32 $(ARCH)" make all  install

withart:
	ARCH="-g -std=c++11 $(ARCH)" make all

distrib:
	mkdir src; cp ifbeam/*.[ch]*  [nuf]*/[^d]*.[ch]* src ; mv src/ifbeam.c src/ifbeam_c.c;  tar czvf nucondb-client.tgz src; rm -rf src
	[ -d Linux* ] || tar czvf ifdhc.tar.gz Makefile bin lib/libifd* lib/python inc/ifdh* inc/[uSW]* inc/num* util ifdh numsg tests ups
	[ -d Linux* ] &&  tar czvf ifdhc.tar.gz Makefile Linux*/bin Linux*/lib/libifd* Linux*/lib/python inc/ifdh* inc/[uSW]* inc/num* util ifdh numsg tests ups || true
	[ -d Linux* ] || tar czvf ifbeam.tar.gz Makefile ifbeam [uf]*/*.[ch]* [iuf]*/Makefile lib/libifb* inc/ifb* inc/[uwW]*  ups `test -r inc/IFBeam_service.h && echo inc/IFBeam_service.h lib/*Beam*`
	[ -d Linux* ] &&  tar czvf ifbeam.tar.gz Makefile ifbeam [uf]*/*.[ch]* [iuf]*/Makefile Linux*/lib/libifb* inc/ifb* inc/[uwW]*  ups `test -r inc/IFBeam_service.h && echo inc/IFBeam_service.h Linux*/lib/*Beam*` || true

