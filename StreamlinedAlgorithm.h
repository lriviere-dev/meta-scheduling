#ifndef SLALGO_H
#define SLALGO_H

#include "Policy.h"
#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Algorithms.h"
#include "BestOfAlgorithm.h"
#include <vector>
#include <iostream>

//Streamlined algorithm 
//outputs list of groupmetasolutions.
class StreamlinedAlgorithm : public Algorithm {
public:

    MetaSolution* solve(const DataInstance& instance) override {
        //Solve instance using CPLEX 
        //todo v2: output a set of diversified good solutions, or at least as set of not too bad solutions
        JSEQSolver JseqSolver(policy);
        MetaSolution* jseq_optim = JseqSolver.solve(instance);

        std::cout << "Solution reached:"; 
        jseq_optim->print();
        std::cout << std::endl << "Score after CPLEX :" << policy->evaluate_meta(*jseq_optim, instance) << std::endl;

        std::vector<SequenceMetaSolution> AllSolutionsSeq;
        AllSolutionsSeq.push_back(*(dynamic_cast<SequenceMetaSolution*>(jseq_optim)));

        std::vector<GroupMetaSolution> AllSolutionsGroup;
        //todo v2: Perturb solutions to add a bunch of them
        //todo v3 : add a bunch of random solutions.

        //Esswein all solutions
        //todo v2 : save every step
        EssweinAlgorithm EWSolver;
        EWSolver.setPolicy(policy);
        std::cout <<  "Number of sequence solutions after first gear : " << AllSolutionsSeq.size() << std::endl;

        for (size_t i=0; i<AllSolutionsSeq.size();i++){
            EWSolver.set_initial_solution(AllSolutionsSeq[i]);
            MetaSolution* ewsol = EWSolver.solve(instance);
            AllSolutionsGroup.push_back(*(dynamic_cast<GroupMetaSolution*>(ewsol)));
            std::cout <<  "ewsol : ";
            ewsol->print();
            std::cout << std::endl;
        }
        std::cout <<  "Esswein step done." << std::endl;


        //Best of all solutions.
        BestOfAlgorithm<GroupMetaSolution> bestof;
        bestof.setPolicy(policy);
        ListMetaSolution<GroupMetaSolution> listgroupmetasol(AllSolutionsGroup);
        bestof.set_initial_solution(listgroupmetasol);
        std::cout <<  "listgroupmetasol";
        listgroupmetasol.print();
        std::cout << std::endl;

        MetaSolution* bestof_output = bestof.solve(instance);
        std::cout<<"bestof output : ";
        bestof_output->print();

        //return best
        return bestof_output; 
    }
};

#endif //SLALGO_H