#ifndef BOALGO_H
#define BOALGO_H

#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Algorithms.h"
#include <vector>
#include <iostream>

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

        // Create a copy of the derived solution
        auto currentSolution = std::make_unique<ListMetaSolution<T>>(*derivedSolution);
        auto bestSolution = std::make_unique<ListMetaSolution<T>>(*currentSolution);

        // Evaluate the initial score
        int bestScore = policy->evaluate_meta(*currentSolution, instance);

        while (currentSolution->get_meta_solutions().size() > 1) {

            int sol_index = policy->find_limiting_element(*currentSolution, instance);
            //std::cout << "\t\tLimiting index : " << sol_index << std::endl;

            // Update current solution (remove the limiting element)
            auto updatedSolution = currentSolution->remove_meta_solution_index(sol_index);
            currentSolution = std::make_unique<ListMetaSolution<T>>(std::move(*updatedSolution));
            // Evaluate new score
            int newScore = policy->evaluate_meta(*currentSolution, instance);


            if (newScore <= bestScore) { // we prefer a smaller output. Note : Doesn't mean there is no redundancy at all nor that list is as small as possible
                bestScore = newScore;
                bestSolution = std::make_unique<ListMetaSolution<T>>(*currentSolution);
            }
        }

        // Return the best solution and release ownership
        return bestSolution.release();
    }
};

#endif //BOALGO_H
