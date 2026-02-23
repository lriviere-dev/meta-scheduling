include ./cpohome

CCC = g++ -g #-pg #pg for profiling
CONCERTDIR = $(CPOHOME)/concert
CPLEXDIR = $(CPOHOME)/cplex

CFLAGS = -std=c++17 -DIL_STD -I$(CPOHOME)/cpoptimizer/include -I$(CONCERTDIR)/include -fPIC -fstrict-aliasing -pedantic -Wall -Wextra -fexceptions -frounding-math -Wno-long-long -m64 -DILOUSEMT -D_REENTRANT -DILM_REENTRANT -Wno-ignored-attributes
LDFLAGS = -L$(CPOHOME)/cpoptimizer/lib/x86-64_linux/static_pic -lcp -L$(CPLEXDIR)/lib/x86-64_linux/static_pic -lcplex -L$(CONCERTDIR)/lib/x86-64_linux/static_pic -lconcert -lpthread -lm -ldl

# SOURCES = $(wildcard *.cpp)  # Automatically find all .cpp files in the current directory    
SOURCES = $(filter-out GenericGA.cpp instanceGenerator.cpp test_instance.cpp RCPSPInstanceGen.cpp, $(wildcard *.cpp))
OBJECTS = $(SOURCES:.cpp=.o) # Convert .cpp filenames to .o filenames


all: program

program: $(OBJECTS)
	$(CCC) -o $@ $(OBJECTS) $(LDFLAGS)


%.o: %.cpp
	$(CCC) -c $(CFLAGS) $< -o $@


%: %.o
	$(CCC) -o $@ $< $(LDFLAGS) #compiles the target file

clean:
	rm -f *.o *.key *.sh program GenericGA test_instance instanceGenerator
