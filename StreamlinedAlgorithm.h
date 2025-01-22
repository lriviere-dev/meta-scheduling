#ifndef SLALGO_H
#define SLALGO_H

#include "Policy.h"
#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Algorithms.h"
#include "BestOfAlgorithm.h"
#include <vector>
#include <iostream>
#include <typeinfo>

//Streamlined algorithm 
//outputs list of groupmetasolutions.
class StreamlinedAlgorithm : public Algorithm {
public:

    StreamlinedAlgorithm(Policy* policy) {
        this->policy = policy; // Use the policy provided during initialization
    }

    MetaSolution* solve(const DataInstance& instance) override {
        JSEQSolver JseqSolver(policy);
        EssweinAlgorithm EWSolver(policy);

        //Solve instance for JSEQ
        //todo v2: output a set of diversified good solutions, or at least as set of not too bad solutions
        MetaSolution* jseq_solution = JseqSolver.solve(instance); // First output : THE JSEQ SOLUTION
        std::cout << "step 0" << std::endl;

        //Esswein on the JSEQ solution
        EWSolver.set_initial_solution(*jseq_solution);
        MetaSolution* gseq_solution = EWSolver.solve(instance); // Second output : THE GSEQ SOLUTION derived from the JSEQ one
        std::cout << "step 1" << std::endl;

        //Diversify JSEQ solution
        std::vector<SequenceMetaSolution> AllSolutionsSeq;
        AllSolutionsSeq.push_back(*(dynamic_cast<SequenceMetaSolution*>(jseq_solution)));
        std::vector<SequenceMetaSolution> diversifiedSeq = diversify_step(AllSolutionsSeq, instance);
        std::cout << "step 2" << std::endl;

        //BestOf on the set of JSEQ solutions
        BestOfAlgorithm<SequenceMetaSolution> bestof_jseq(policy);
        ListMetaSolution<SequenceMetaSolution> listseqmetasol(diversifiedSeq);
        bestof_jseq.set_initial_solution(listseqmetasol);
        MetaSolution* sjseq_solution = bestof_jseq.solve(instance); // Third output : THE SJSEQ SOLUTION derived from the set of JSEQ solutions
        std::cout << "step 3" << std::endl;

        //Esswein on the set of JSEQ solutions (and keep intermediate solutions)
        std::vector<GroupMetaSolution> AllSolutionsGroup;
        for (size_t i=0; i<diversifiedSeq.size();i++){
            EWSolver.set_initial_solution(diversifiedSeq[i]);
            std::vector<MetaSolution*> ewsols = EWSolver.solve_savesteps(instance);
            for (size_t j = 0; j<ewsols.size(); j++)
            {  
                AllSolutionsGroup.push_back(*(dynamic_cast<GroupMetaSolution*>(ewsols[j])));
            }
        }
        std::cout << "step 4" << std::endl;

        //BestOf on the set of GSEQ solutions
        BestOfAlgorithm<GroupMetaSolution> bestof_gseq(policy);
        ListMetaSolution<GroupMetaSolution> listgroupmetasol(AllSolutionsGroup);
        bestof_gseq.set_initial_solution(listgroupmetasol);
        MetaSolution* sgseq_solution = bestof_gseq.solve(instance); // Fourth output : THE SGSEQ SOLUTION derived from the set of GSEQ solutions
        std::cout << "step 5" << std::endl;

        // Print the 4 solutions reached
        std::vector<MetaSolution*> outputs = {jseq_solution, gseq_solution, sjseq_solution, sgseq_solution};
        for (size_t i = 0; i<outputs.size(); i++){
            std::cout << "Output " << i << ": " << typeid(*outputs[i]).name() << std::endl;
            std::cout << "Solution: ";
            std::cout << std::endl << "Score: " << policy->evaluate_meta(*outputs[i], instance) << std::endl << std::endl;        
        }

        return sgseq_solution; //as an algorithm, only returns one output, that is supposed to be the best. but any of the fourth output could be relevant.
    }

private :
    //the function in charge of diversifying the JSEQ solution pool.
    //Assumes the input list contains high quality solutions
    //then use random diversification steps with increasing strenghth (parameterized)
    //outputs a pool of admissible JSEQ solutions (precedences are checked)
    // Note : FIFO and other local decisions heuristic can handle solutions containing invalid solutions in theory, but my code might need tweaks to do so.
   std::vector<SequenceMetaSolution> diversify_step (const std::vector<SequenceMetaSolution>& input_solutions, const DataInstance& instance) {
        int steps = 1; //TODO : could be a parameter.
        int neighborhood_size = 1; //TODO : could be a parameter.
        std::vector<SequenceMetaSolution> output_solutions;

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
};

#endif //SLALGO_H