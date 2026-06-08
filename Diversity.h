#ifndef DIVERSITY_H
#define DIVERSITY_H

//This file defines a few methods to generate diverse solutions based on a sequence solution, or several ones.

#include <iostream>
#include <string>
#include <stdexcept> 

#include "Schedule.h"
#include "Instance.h"
#include "Sequence.h"
#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Algorithms.h"
#include "BestOfAlgorithm.h"
#include "Ideal.h"
#include "Timer.h"

#include "PolicyRCPSP.h"


std::vector<SequenceMetaSolution> diversify_step_neighbours (std::vector<SequenceMetaSolution>& input_solutions, const DataInstance& instance) {
    int steps = 1; //TODO : could be a parameter.
    int neighborhood_size = 1; //TODO : could be a parameter.
    std::vector<SequenceMetaSolution> output_solutions;
    output_solutions.insert(output_solutions.end(), input_solutions.begin(), input_solutions.end()); //insert the original solutions
    //return output_solutions; //TODO : shortcutting process to not diversify.
    
    for (int step = 0; step < steps; step++) //repeat process depending on diversification strenghth (this will create duplicate solutions as inverse step can be taken. Could be done smarter)
    {
        for (size_t i = 0; i < input_solutions.size(); i++)
        {
            SequenceMetaSolution current = input_solutions[i];
            std::vector<SequenceMetaSolution> neighbors = current.gen_neighbors(neighborhood_size, instance); //instance is passed down to check precedence constraints
            output_solutions.insert(output_solutions.end(), neighbors.begin(), neighbors.end());
        }
    }
    return output_solutions;
}

std::vector<SequenceMetaSolution> diversify_step_random (std::vector<SequenceMetaSolution>& input_solutions, const DataInstance& instance,  std::mt19937 &rng) {
    std::vector<SequenceMetaSolution> output_solutions;
    output_solutions.insert(output_solutions.end(), input_solutions.begin(), input_solutions.end()); //insert the original solutions
    int k = 2;

    //SwapDescent descent(&policy);
    size_t nb_new = pow(instance.getN(),k); //k controls number of random solutions
    for (size_t i = 0; i < nb_new; i++)
    {
        Sequence rand_seq = Sequence(instance.getN(), rng);//gen random sequence
        rand_seq.fix_precedence_constraints(instance); //fix it for prec constraints
        SequenceMetaSolution rand_seq_meta = SequenceMetaSolution(rand_seq);
        //descent.set_initial_solution(rand_seq_meta);
        output_solutions.push_back(rand_seq_meta);//add to list
        //MetaSolution* descended_seq_meta = descent.solve(instance);//descent on it
        //output_solutions.push_back(*(dynamic_cast<SequenceMetaSolution*>(descended_seq_meta)));
    }
    
    return output_solutions;
}

std::vector<SequenceMetaSolution> diversify_step_jseq (std::vector<SequenceMetaSolution>& input_solutions, const DataInstance& instance, JSEQSolver & jseqsolver) {
    
    int max_size = 100;
    
    std::vector<SequenceMetaSolution> output_solutions;
    output_solutions.insert(output_solutions.end(), input_solutions.begin(), input_solutions.end()); //insert the original solutions
    
    //solve again using solve_savestep (redundant of course, just for testing purposes)
    for (MetaSolution* metasol : jseqsolver.solve_steps(instance)){
        output_solutions.push_back(*(dynamic_cast<SequenceMetaSolution*>(metasol)));
    }

    // Keep only the last k elements
    if (output_solutions.size() > max_size) {
        output_solutions.erase(output_solutions.begin(), output_solutions.begin() + (output_solutions.size() - max_size));
    }
    return output_solutions;
}

std::vector<SequenceMetaSolution> diversify_step_ideal (std::vector<SequenceMetaSolution>& input_solutions, const DataInstance& instance, MetaSolution * ideal_sol) {
    std::vector<SequenceMetaSolution> output_solutions;
    output_solutions.insert(output_solutions.end(), input_solutions.begin(), input_solutions.end()); //insert the original solutions
    
    //get the computed sequence for each scenario computed in the idealsol. (remember they are unlikely to be the front too)
    for (Sequence seq : (dynamic_cast<IdealMetaSolution*> (ideal_sol))->get_sequences()){
        SequenceMetaSolution x = SequenceMetaSolution(seq);
        if(std::find(output_solutions.begin(), output_solutions.end(), x) == output_solutions.end()) {//not adding doubles
            output_solutions.push_back(x);
        }
    }
    return output_solutions;
}

//diversify that combines other functions. the idea is that we want a rather large number of solutions. the better a solution is, the more we add it's neighbours.
//We could evaluate them mid-diversification to keep even more qualitative solutions.
std::vector<SequenceMetaSolution> diversify_step_multi (std::vector<SequenceMetaSolution>& input_solutions, const DataInstance& instance, MetaSolution * ideal_sol, std::mt19937 &rng) {
    std::vector<SequenceMetaSolution> output_solutions;
    //add initial solutions
    output_solutions.insert(output_solutions.end(), input_solutions.begin(), input_solutions.end()); //insert the original solutions
    //extra diversify by integrating neighbors of previously added "good quality" solutions.
    output_solutions = diversify_step_neighbours(output_solutions, instance);

    //add jseq research solution (don't do it because I don't want to run a solver a long time again, but we could keep the sequences from earlier)
    // using diversify_step_jseq

    //add ideal solutions (the "best" sequence found in each scenario without front considerations (as per limited solver time)) //This is arguably cheating, as it is an NP hard process, but we use a fixed time to find them.
    output_solutions = diversify_step_ideal(output_solutions, instance, ideal_sol);

    //extra diversify by integrating neighbors of previously added "good quality" solutions.
    output_solutions = diversify_step_neighbours(output_solutions, instance);

    //add random solutions (don't add their neighbours) //However, this introduces lots of variability to the result. Maybe skip ?
    //output_solutions = diversify_step_random(output_solutions, instance, rng);

    return output_solutions;
}




#endif // DIVERSITY_H