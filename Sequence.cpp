#include "Sequence.h"
#include "Instance.h"

// Constructor implementation
Sequence::Sequence(const std::vector<int>& tasks) : tasks(tasks) {}

// Print function implementation
void Sequence::print() const {
    for (int i = 0; i<tasks.size(); i++) {
        if (i != 0){  // this works because tasks should be unique in a sequence.
            std::cout << "*";
        }
        std::cout << tasks[i];
    }
}

//checks precedence validity
bool Sequence::check_precedence_constraints(const DataInstance& instance) const {
    const std::vector<int>& tasks = get_tasks();
    const std::vector<std::vector<int>>& precedenceConstraints = instance.precedenceConstraints;


    // Check each precedence constraint
    // Check each pair (i, j) where precedenceConstraints[i][j] == 1
    for (size_t i = 0; i < tasks.size()-1; ++i) {
        for (size_t j = i+1; j < tasks.size(); ++j) {
            if (precedenceConstraints[tasks[j]][tasks[i]] == 1) {
                // Task j must precede task i, but it doesn't
                return false;
            }
        }
    }

    return true; // All constraints are respected
}

//compares two sequences to find the preffered one by FIFO in a given scenario
bool Sequence::isLexicographicallySmaller(const Sequence& other, const DataInstance& scenario) const {
    const auto& releaseDates = scenario.releaseDates[0]; // Assuming single-machine case

    // Compare task order in sequences
    for (size_t i = 0; i < tasks.size(); ++i) { 
                int taskA = tasks[i];
        int taskB = other.tasks[i];

        if (releaseDates[taskA] != releaseDates[taskB]) {
            // Earlier release date wins
            return releaseDates[taskA] < releaseDates[taskB];
        }

        // If release dates are equal, compare task indices lexicographically
        if (taskA != taskB) {
            return taskA < taskB;
        }
    }

    // At this point, sequences should be equal. So not strictly smaller
    return false;
}

// Accessor implementation
const std::vector<int>& Sequence::get_tasks() const {
    return tasks;
}
