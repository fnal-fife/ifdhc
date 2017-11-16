SRCDIR=../../numsg
VPATH=$(SRCDIR)

BIN=numsg-test nulogprintf
LIB=LibNuMsg.a
UTLOBJ=../util/SyslogAPI.o ../util/utils.o ../util/WimpyConfigParser.o
OBJ=numsg.o
HDR=numsg.h
LOG_OBJ= nulogprintf.o $(OBJ)
CXXFLAGS=-pedantic-errors -Wall  -Wextra -Werror -fpic -g $(ARCH) -I../util -I../../util -I$(SRCDIR)

VPATH=../../numsg

all: $(OBJ) $(BIN) $(LIB)

install:
	test -d ../lib || mkdir -p ../lib
	test -d ../include || mkdir -p ../include
	cp $(LIB) ../lib
	cp $(HDR) ../include

clean:
	rm -f *.o *.a $(BIN) $(LIB)

nulogprintf: $(LOG_OBJ)
	$(CXX) $(CXXFLAGS) -o nulogprintf $(LOG_OBJ) $(UTLOBJ)

LibNuMsg.a: $(OBJ) $(UTLOBJ)
	ar qv $(LIB) $(OBJ) $(UTLOBJ)
        
numsg-test: $(UTLOBJ) numsg.cc
	$(CXX) $(CXXFLAGS) -DUNITTEST $^ -o numsg-test

%.o: %.cc
	g++ -c -o $@  $(CXXFLAGS) $< 

nulogprintf.o: nulogprintf.cc numsg.h ../util/SyslogAPI.h 
numsg.o: ../util/utils.h 
