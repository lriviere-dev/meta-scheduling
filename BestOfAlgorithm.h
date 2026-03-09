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
        size_t bestFrontSize = currentSolution.get_front_size();

        size_t nb_submetasols = currentSolution.get_meta_solutions_size();

        //Create arrays to accelerate BO
        //array storing priority queue for each scenario (submetasolutions lexicographically smaller)
        std::vector<std::queue<size_t>> scenarios_priority_indexes(instance.getS()); //after initialization, stores ordered indexes of the submetasolution list
        std::vector<size_t> tmp(nb_submetasols); //tmp vector to store sorts
        std::iota(tmp.begin(), tmp.end(), 0); // Fill 0..N-1 once. (out of loop because elements aren't changed)
        std::vector<MetaSolution*> ms = currentSolution.get_meta_solutions();

        for (size_t s = 0; s < instance.getS(); ++s) { //initialize for each scenario with indexes sorted by priority 
            // Sort with a lambda (descending)
            std::sort(tmp.begin(), tmp.end(), [ms, this, &currentSolution,&s, &instance](size_t a, size_t b) {
                return policy->isLexicographicallySmaller(ms[a]->front_sequences[s], ms[b]->front_sequences[s], instance, s); 
            });
            // Remplir la queue dans les deux cas
            for (size_t idx : tmp) scenarios_priority_indexes[s].push(idx);

            // Note that due to sorting equalities, it may happen that the existing front_index data on the solution points  to  a different metaSolution with the same front_sequence, and hence the same score too. 
            // this led to bugs, and loss of a couple days of my life.
            // To fix this, we reassign the front_indexes according to the new priority queues.
            // The resulting current_solution is equivalent to the initial one.
            // Further updates are handled through the queue only hence do not pose coherence problems
            currentSolution.front_indexes[s] = scenarios_priority_indexes[s].front();
        }

        
        //Arrays used to keep track of moving indexes 
        std::vector<int> position_of_indexes; //array[i] gives position (index in current sol) of index i (in original solution : the one used in scenarios_priority_indexes)
        position_of_indexes.resize(nb_submetasols);
        for (int i = 0; i < nb_submetasols; ++i) position_of_indexes[i] = i;

        std::vector<size_t> reverse_positions; //reverse of positions (used to find content at each index)
        reverse_positions.resize(nb_submetasols);
        for (size_t i = 0; i < nb_submetasols; ++i) reverse_positions[i] = i;
        

        while (currentSolution.get_meta_solutions().size() > 1) {

            int limiting_scenario = policy->find_limiting_scenario(currentSolution, instance);
            int sol_index = currentSolution.front_indexes[limiting_scenario]; 
            // Update current solution (remove the limiting element)
            currentSolution.remove_meta_solution_index_update(sol_index, scenarios_priority_indexes, position_of_indexes , reverse_positions );
            removes.push_back(sol_index);//saving the removed solution index to build it back when needed
            // Evaluate new score
            int newScore = currentSolution.score; //carefull, Doesn't actually reevaluate because we updated scores by hand !
            size_t newFrontSize = currentSolution.get_front_size();

            if (newScore <bestScore || (newScore==bestScore && newFrontSize < bestFrontSize)) { // we prefer a smaller front size (implies more robust sequences) output. Note : Doesn't mean there is no redundancy at all nor that list is as small as possible
                bestScore = newScore;
                bestRemoves = removes;
                bestFrontSize = newFrontSize;
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

template <typename T>
class BestKGreedyAlgorithm : public SecondStageAlgorithm {

public:

    BestKGreedyAlgorithm<T>(Policy* policy) {
        this->policy = policy; // Use the policy provided during initialization
    }

    void set_k(int k) {
        if (k <= 0) {
            throw std::invalid_argument("k must be a positive integer.");
        }

        this->k = k;
    }

    /**
     * Greedy forward selection: 
     * Starts with an empty set and adds the metasolution that provides the 
     * greatest improvement to the robust bottleneck score until size k is reached.
     */
    MetaSolution* solve(const DataInstance& instance) {
        if (!policy) throw std::runtime_error("Policy not set.");
        if (!initial_solution) throw std::runtime_error("Initial solution must be set before running the algorithm.");
        ListMetaSolution<T>* listMetaSolution = dynamic_cast<ListMetaSolution<T>*>(initial_solution);
        if (!listMetaSolution) throw std::runtime_error("Initial_solution must be of type ListMetaSolution<T>.");

        std::vector<MetaSolution*> candidates = listMetaSolution->get_meta_solutions();
        std::vector<bool> used(candidates.size(), false); // Track which candidates have been added to the solution
        
        // The accumulator: starts empty
        // We initialize it as an empty list of metasolutions
        ListMetaSolution<T>* accu = new ListMetaSolution<T>(std::vector<T>{});

        for (int i = 0; i < k && i < (int)candidates.size(); ++i) {
            int bestCandidateIdx = -1;
            int bestScore = std::numeric_limits<int>::max();

            // Search for the best metasolution to add to the current accu
            for (size_t c = 0; c < candidates.size(); ++c) {
                if (used[c]) continue; //skip already added solutions.

                // Create a temporary version to evaluate the addition
                ListMetaSolution<T> testSol = *accu; // If too long, optimize by not reevaluating this without a full copy 
                testSol.add_meta_solution(*static_cast<T*>(candidates[c]));
                int currentScore = policy->evaluate_meta(testSol, instance);

                if (currentScore < bestScore) {
                    bestScore = currentScore;
                    bestCandidateIdx = c;
                }
            }

            // Add the best one found in this iteration to our accumulator
            accu->add_meta_solution(*static_cast<T*>(candidates[bestCandidateIdx]));
            used[bestCandidateIdx] = true;
        }

        return accu;
    }
    
private:
    int k;

};

template <typename T>
class BestKGreedyAlgorithm2 : public SecondStageAlgorithm {

public:

    BestKGreedyAlgorithm2<T>(Policy* policy) {
        this->policy = policy; // Use the policy provided during initialization
    }

    void set_k(int k) {
        if (k <= 0) {
            throw std::invalid_argument("k must be a positive integer.");
        }
        this->k = k;
    }

    /**
     * Greedy but starts from the front of bestof selection: 
     * REMOVES the least decreasing solution from the bestof solution iteratively until size k is reached. 
     * greatest improvement to the robust bottleneck score until size k is reached.
     */
MetaSolution* solve(const DataInstance& instance) override {
        if (!policy) throw std::runtime_error("Policy not set.");
        if (!initial_solution) throw std::runtime_error("Initial solution must be set.");
        ListMetaSolution<T>* listMetaSolution = dynamic_cast<ListMetaSolution<T>*>(initial_solution);
        if (!listMetaSolution) throw std::runtime_error("Initial_solution must be of type ListMetaSolution<T>.");

        // We start with the full set of metasolutions
        ListMetaSolution<T>* currentSol = new ListMetaSolution<T>(*listMetaSolution);

        // Continue removing until we reach k
        while (currentSol->get_meta_solutions_size() > (size_t)k ) {
            int bestCandidateToRemove = -1;
            int bestScoreFound = std::numeric_limits<int>::max();

            // We iterate through current indices to find the "least useful" metasolution
            size_t currentSize = currentSol->get_meta_solutions_size();
            for (size_t i = 0; i < currentSize; ++i) {
                // Create a temporary copy to evaluate the state after removal
                ListMetaSolution<T> testSol = *currentSol;
                int currentScore; //score of that solution

                // Remove the i-th metasolution
                // Note: Ensure your ListMetaSolution has a method to remove by index
                testSol.remove_meta_solution_index(i); 
                testSol.reset_evaluation(); // Ensure we reset evaluation to get correct score after modification (could be optimized)
                // int currentScore = policy->evaluate_meta(testSol, instance);
                try {
                    currentScore = policy->evaluate_meta(testSol, instance, bestScoreFound); //eval, but get out if score is bad
                } catch (const EvaluationBoundExceeded& e) {
                    continue; //skip to next candidate
                }


                // We want to keep the subset that has the MINIMUM bottleneck score
                if (currentScore < bestScoreFound) {
                    bestScoreFound = currentScore;
                    bestCandidateToRemove = i;
                }
            }

            // Perform the best removal on our actual working solution
            if (bestCandidateToRemove != -1) {
                currentSol->remove_meta_solution_index(bestCandidateToRemove);
            } else {
                break; // but should never happen 
            }
        }

        return currentSol;
    }

private:
    int k;

};

#endif //BOALGO_H
