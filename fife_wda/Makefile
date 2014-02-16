SRCDIR=../../fife_wda/
VPATH=$(SRCDIR)
GEN=wda_version.h
LIB=libwda.a
HDR=ifbeam_c.h wda.h
OBJ=ifbeam.o wda.o
SRC= ifbeam.c wda.c
TST=test_ifbeam
CXXFLAGS=-fPIC -g -O3 $(DEFS) $(ARCH) -I$(SRCDIR) -I.


all: $(GEN) $(BIN) $(TST) $(LIB) $(SHLIB)

install:
	test -d ../lib || mkdir ../lib
	test -d ../include/wda || mkdir -p ../include/wda
	cp $(LIB) ../lib
	cp $(HDR) ../include

clean:
	rm -f *.o *.a 

$(LIB): $(OBJ) $(UTLOBJ)
	rm -f $(LIB)
	ar qv $(LIB) $(OBJ) 

wda_version.h: FORCE
	echo '#define WDA_VERSION "'`git describe --tags --match 'wda*'`'"' > $@

FORCE:

%.o: %.c
	gcc -c -o $@ $(CXXFLAGS) $< 

test_ifbeam: test_ifbeam.o $(OBJ)
	gcc -o ifbeam_test test_ifbeam.o $(OBJ) `test -r /usr/lib/libcurl.a && echo -Wl,-Bstatic` -lcurl -llber -lldap -Wl,-Bdynamic -lidn -lssl
