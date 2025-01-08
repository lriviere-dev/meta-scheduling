#ifndef SLALGO_H
#define SLALGO_H


#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Algorithms.h"
#include <vector>
#include <iostream>

//Streamlined algorithm 
//outputs list of groupmetasolutions.
class StreamlinedAlgorithm : public Algorithm {
public:

    MetaSolution* solve(const DataInstance& instance) override {
        //Solve instance using CPLEX 
        //todo v2: output a set of diversified good solutions, or at least as set of not too bad solutions
        
        
        //todo v2: Perturb solutions to add a bunch of them

        //todo v3 : add a bunch of randomo solutions.

        //Esswein all solutions
        //todo v2 : save every step

        //Best of all solutions.
        
        //return best
        return nullptr; //TODO
    }
};

#endif //SLALGO_H