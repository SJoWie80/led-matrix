CFLAGS=-Wall -O3 -g
CXXFLAGS=-Wall -O3 -g
OBJECTS=main.o gpio.o led-matrix.o thread.o
BINARIES=led-matrix
LDFLAGS=-lrt -lm -lpthread -lgif

all : $(BINARIES)

led-matrix.o: led-matrix.cc led-matrix.h
main.o: led-matrix.h

led-matrix : $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJECTS) $(BINARIES)
