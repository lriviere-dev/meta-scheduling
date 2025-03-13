include ./cpohome

CCC = g++ -g #-pg 
CONCERTDIR = $(CPOHOME)/concert
CPLEXDIR = $(CPOHOME)/cplex

CFLAGS = -DIL_STD -I$(CPOHOME)/cpoptimizer/include -I$(CONCERTDIR)/include -fPIC -fstrict-aliasing -pedantic -Wall -Wextra -fexceptions -frounding-math -Wno-long-long -m64 -DILOUSEMT -D_REENTRANT -DILM_REENTRANT -Wno-ignored-attributes
LDFLAGS = -L$(CPOHOME)/cpoptimizer/lib/x86-64_linux/static_pic -lcp -L$(CPLEXDIR)/lib/x86-64_linux/static_pic -lcplex -L$(CONCERTDIR)/lib/x86-64_linux/static_pic -lconcert -lpthread -lm -ldl

SOURCES = $(wildcard *.cpp)  # Automatically find all .cpp files in the current directory
OBJECTS = $(SOURCES:.cpp=.o) # Convert .cpp filenames to .o filenames

help:
	echo "use [make name] to compile name.cpp into the name executable file"

all: program

program: $(OBJECTS)
	$(CCC) -o $@ $(OBJECTS) $(LDFLAGS)

%.o: %.cpp
	$(CCC) -c $(CFLAGS) $< -o $@

%: %.o
	$(CCC) -o $@ $< $(LDFLAGS) #compiles the target file

clean:
	rm -f *.o *.key *.sh program
