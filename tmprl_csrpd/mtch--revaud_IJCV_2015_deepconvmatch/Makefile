CC=g++_48

OS_NAME=$(shell uname -s)
ifeq ($(OS_NAME),Linux)
  #LAPACKLDFLAGS=/usr/lib64/atlas/libsatlas.so   # single-threaded blas
  #LAPACKLDFLAGS=/usr/lib64/atlas/libtatlas.so  # multi-threaded blas
  BLAS_THREADING=-D MULTITHREADED_BLAS # remove this if wrong
  LAPACKLDFLAGS=/usr/lib64/atlas/libptf77blas.so.3  # multi-threaded blas
endif
ifeq ($(OS_NAME),Darwin)  # Mac OS X
  LAPACKLDFLAGS=-framework Accelerate # for OS X
endif
LAPACKCFLAGS=-Dinteger=int $(BLAS_THREADING)
STATICLAPACKLDFLAGS=-fPIC -Wall -g -fopenmp -static -static-libstdc++ /home/lear/douze/tmp/jpeg-6b/libjpeg.a /usr/lib64/libpng.a /usr/lib64/libz.a /usr/lib64/libblas.a /usr/lib/gcc/x86_64-redhat-linux/4.9.2/libgfortran.a /usr/lib/gcc/x86_64-redhat-linux/4.9.2/libquadmath.a # statically linked version

CFLAGS= -fPIC -Wall -g -std=c++11 $(LAPACKCFLAGS) -fopenmp -DUSE_OPENMP -O3 $(shell pkg-config --static --cflags --libs opencv)
LDFLAGS=-fPIC -Wall -g -ljpeg -lpng -fopenmp $(shell pkg-config --static --cflags --libs opencv) 

SOURCES := $(shell find . -name '*.cpp')
OBJ := $(SOURCES:%.cpp=%.o)
HEADERS := $(shell find . -name '*.h')

all: deepmatching

.cpp.o:  %.cpp %.h
	$(CC) -o $@ $(CFLAGS) -c $+

deepmatching: $(HEADERS) $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LAPACKLDFLAGS)

deepmatching-static: $(HEADERS) $(OBJ)
	$(CC) -o $@ $^ $(STATICLAPACKLDFLAGS)


clean:
	rm -f $(OBJ) deepmatching *~ *.pyc .gdb_history
