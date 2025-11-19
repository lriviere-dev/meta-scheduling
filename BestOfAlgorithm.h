#ifndef BOALGO_H
#define BOALGO_H

#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Algorithms.h"
#include <vector>
#include <iostream>
#include "Timer.h"

//THe best of algorithm (metaversion)
template <typename T>
class BestOfAlgorithm : public SecondStageAlgorithm {
public:
    BestOfAlgorithm<T>(Policy* policy) {
        this->policy = policy; // Use the policy provided during initialization
    }

    MetaSolution* solve(const DataInstance& instance) override {
        // Ensure the policy is set
        if (!policy) {
            throw std::runtime_error("Policy must be set before running the algorithm.");
        }

        // Ensure the initial solution is set
        if (!initial_solution) {
            throw std::runtime_error("Initial solution must be set before running the algorithm.");
        }

        // Ensure the initial solution is of the correct polymorphic type
        auto listMetaSolution = dynamic_cast<ListMetaSolutionBase*>(initial_solution);
        if (!listMetaSolution) {
            throw std::runtime_error("Initial solution must be of type ListMetaSolutionBase.");
        }

        // Create current solution and best solution polymorphically
        // Here we assume dynamic_cast worked and the underlying type is ListMetaSolution<T>
        auto derivedSolution = dynamic_cast<ListMetaSolution<T>*>(listMetaSolution);
        if (!derivedSolution) {
            throw std::runtime_error("Initial solution must be of type ListMetaSolution<T>.");
        }

        // Create a copy of the derived solution that we can then modify inplace
        ListMetaSolution<T> currentSolution = ListMetaSolution<T>(*derivedSolution); //calling copy constructor
        std::vector<int> bestRemoves;
        std::vector<int> removes;

        // Evaluate the initial score
        int bestScore = policy->evaluate_meta(currentSolution, instance);

        size_t nb_submetasols = currentSolution.get_meta_solutions_size();

        //Create arrays to accelerate BO
        //array storing priority queue for each scenario (submetasolutions lexicographically smaller)
        std::vector<std::queue<size_t>> scenarios_priority_indexes(instance.S); //after initialization, stores ordered indexes of the submetasolution list
        std::vector<size_t> tmp(nb_submetasols); //tmp vector to store sorts
        std::iota(tmp.begin(), tmp.end(), 0); // Fill 0..N-1 once. (out of loop because elements aren't changed)
        std::vector<MetaSolution*> ms = currentSolution.get_meta_solutions();

        for (size_t s = 0; s < instance.S; ++s) { //initialize for each scenario with indexes sorted by priority // --- s = 0 : tri complet ---
            // Sort with a lambda (descending)
            std::sort(tmp.begin(), tmp.end(), [ms, this, &currentSolution,&s, &instance](size_t a, size_t b) {
                return policy->isLexicographicallySmaller(ms[a]->front_sequences[s], ms[b]->front_sequences[s], instance, s); 
            });
            // Remplir la queue dans les deux cas
            for (size_t idx : tmp) scenarios_priority_indexes[s].push(idx);
        }

        

        
        //Arrays used to keep track of moving indexes
        std::vector<std::vector<int>> scenarios_position_of_indexes(instance.S); //array[s,i] gives position (index in current sol) of index i (in original solution : the one used in scenarios_priority_indexes)
        for (size_t s = 0; s < instance.S; ++s) {//instanciation with integers from zero to the number of submetasols for each scenario
            scenarios_position_of_indexes[s].resize(nb_submetasols);
            for (int i = 0; i < nb_submetasols; ++i) scenarios_position_of_indexes[s][i] = i;
        }

        std::vector<std::vector<size_t>> scenarios_reverse_positions(instance.S); //reverse of positions (used to find content at each index)
        for (size_t s = 0; s < instance.S; ++s) {//instanciation with integers from zero to the number of submetasols
            scenarios_reverse_positions[s].resize(nb_submetasols);
            for (size_t i = 0; i < nb_submetasols; ++i) scenarios_reverse_positions[s][i] = i;
        }

        while (currentSolution.get_meta_solutions().size() > 1) {

            int limiting_scenario = policy->find_limiting_scenario(currentSolution, instance);
            int sol_index = currentSolution.front_indexes[limiting_scenario]; 
            //std::cout << "\t\tLimiting index : " << sol_index << std::endl;

            // Update current solution (remove the limiting element)
            currentSolution.remove_meta_solution_index_update(sol_index, scenarios_priority_indexes, scenarios_position_of_indexes , scenarios_reverse_positions );
            removes.push_back(sol_index);//saving the removed solution index to build it back when needed
            // Evaluate new score
            int newScore = currentSolution.score; //carefull, Doesn't actually reevaluate because we updated scores by hand !


            if (newScore <= bestScore) { // we prefer a smaller output. Note : Doesn't mean there is no redundancy at all nor that list is as small as possible
                bestScore = newScore;
                bestRemoves = removes;
            }
        }

        //build solution back using simple removes, doesn't re-evaluate (will be done once next time eval meta.)
        ListMetaSolution<T>* outputSol = new ListMetaSolution<T>(*derivedSolution); //recreate a copy of the best solution
        for(size_t i =0; i<bestRemoves.size();i++){ //remove appropriate elements
            outputSol->remove_meta_solution_index(bestRemoves[i]);
        }
        return outputSol;
    }

};

#endif //BOALGO_H
