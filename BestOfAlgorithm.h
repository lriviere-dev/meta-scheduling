#ifndef BOALGO_H
#define BOALGO_H

#include "MetaSolutions.h"
#include "ListMetaSolutions.h"
#include "Algorithms.h"
#include <vector>
#include <iostream>
#include "Timer.h"

// BestOfAlgorithm is no longer templated — works on any ListMetaSolution
// (mixed or homogeneous) through the ListMetaSolutionBase interface.
class BestOfAlgorithm : public SecondStageAlgorithm {
public:
    BestOfAlgorithm(Policy* policy) {
        this->policy = policy;
    }

    MetaSolution* solve(const DataInstance& instance) override {
        if (!policy)
            throw std::runtime_error("Policy must be set before running the algorithm.");
        if (!initial_solution)
            throw std::runtime_error("Initial solution must be set before running the algorithm.");

        auto* listMeta = dynamic_cast<ListMetaSolutionBase*>(initial_solution);
        if (!listMeta)
            throw std::runtime_error("Initial solution must be of type ListMetaSolutionBase.");

        // Deep clone — owns all sub-metasolutions, evaluation data included
        ListMetaSolution currentSolution = *dynamic_cast<ListMetaSolution*>(listMeta);

        std::vector<int> bestRemoves;
        std::vector<int> removes;

        int bestScore     = policy->evaluate_meta(currentSolution, instance);
        size_t bestFrontSize = currentSolution.get_front_size();
        size_t nb_submetasols = currentSolution.get_meta_solutions_size();

        // Priority queues per scenario, sorted by policy order
        std::vector<std::queue<size_t>> scenarios_priority_indexes(instance.getS());
        std::vector<size_t> tmp(nb_submetasols);
        std::iota(tmp.begin(), tmp.end(), 0);

        std::vector<MetaSolution*> ms = currentSolution.get_meta_solutions(); // view-only snapshot

        for (size_t s = 0; s < static_cast<size_t>(instance.getS()); ++s) {
            std::sort(tmp.begin(), tmp.end(), [&ms, this, &currentSolution, &s, &instance](size_t a, size_t b) {
                return policy->isLexicographicallySmaller(
                    ms[a]->front_sequences[s],
                    ms[b]->front_sequences[s],
                    instance, s);
            });
            for (size_t idx : tmp) scenarios_priority_indexes[s].push(idx);
            // Realign front_indexes to match sorted order (avoids coherence bugs from tie-breaking)
            currentSolution.front_indexes[s] = scenarios_priority_indexes[s].front();
        }
        // ms snapshot no longer needed after this point — don't use it below

        std::vector<int> position_of_indexes(nb_submetasols);
        std::iota(position_of_indexes.begin(), position_of_indexes.end(), 0);

        std::vector<size_t> reverse_positions(nb_submetasols);
        std::iota(reverse_positions.begin(), reverse_positions.end(), 0);

        while (currentSolution.get_meta_solutions_size() > 1) {
            int limiting_scenario = policy->find_limiting_scenario(currentSolution, instance);
            int sol_index = currentSolution.front_indexes[limiting_scenario];

            currentSolution.remove_meta_solution_index_update(
                sol_index, scenarios_priority_indexes, position_of_indexes, reverse_positions);
            removes.push_back(sol_index);

            int newScore      = currentSolution.score; // already updated in-place
            size_t newFrontSize = currentSolution.get_front_size();

            if (newScore < bestScore || (newScore == bestScore && newFrontSize < bestFrontSize)) {
                bestScore     = newScore;
                bestFrontSize = newFrontSize;
                bestRemoves   = removes;
            }
        }

        // Reconstruct the best solution from the original by replaying removals
        ListMetaSolution* outputSol = dynamic_cast<ListMetaSolution*>(listMeta)->clone();
        for (size_t i = 0; i < bestRemoves.size(); i++)
            outputSol->remove_meta_solution_index(bestRemoves[i]);
        return outputSol;
    }
};

#endif // BOALGO_H