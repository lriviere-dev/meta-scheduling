#include "Sequence.h"
#include "Instance.h"

// Constructor implementation
Sequence::Sequence(const std::vector<int>& tasks) : tasks(tasks) {}

Sequence::Sequence(){}

// Print function implementation
void Sequence::print() const {
    for (size_t i = 0; i<tasks.size(); i++) {
        if (i != 0){  // this works because tasks should be unique in a sequence.
            std::cout << "-";
        }
        std::cout << tasks[i];
    }
}

//checks precedence validity
bool Sequence::check_precedence_constraints(const DataInstance& instance) const {
    const std::vector<int>& tasks = get_tasks();
    const std::vector<uint8_t>& precedenceConstraints = instance.precedenceConstraints;


    // Check each precedence constraint
    // Check each pair (i, j) where precedenceConstraints[i][j] == 1
    for (size_t i = 0; i < tasks.size()-1; ++i) {
        for (size_t j = i+1; j < tasks.size(); ++j) {
            if (precedenceConstraints[tasks[j]*instance.N+tasks[i]] == 1) {
                // Task j must precede task i, but it doesn't
                return false;
            }
        }
    }

    return true; // All constraints are respected
}

//compares two sequences to find the preffered one by FIFO in a given scenario 
//TODO : Warning: should depend on policy ! (the theory here needs to be layed out)
bool Sequence::isLexicographicallySmaller(const Sequence& other, const DataInstance& instance, int scenario_id) const {
    const auto& releaseDates = instance.releaseDates[scenario_id]; // Assuming single-machine case

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


 std::vector<Sequence> Sequence::neighbours(int neighborhood_size){
    //assert valid neighborhood size input.
    // The 3 neighborhoods successively dominate eachothers
    // 1 : adjacent swaps (size N)
    // 2 : inserts (size N^2)
    // 3 : cut and insert (I think N^3) TODO

    std::vector<Sequence> output;
    std::vector<int> tasks = this->get_tasks();

    if (neighborhood_size == 1){ // swaps
        for (size_t i = 0; i < tasks.size()-1; i++)
        {
            std::vector<int> swaped = tasks; 
            std::swap(swaped[i], swaped[i + 1]);
            output.push_back(Sequence(swaped));
        }
     
    }
    else if (neighborhood_size == 2) { // reinsertions
        for (size_t i = 0; i < tasks.size(); i++) {
            int task = tasks[i];
            for (size_t j = 0; j < i; j++) { // Insert before i
                std::vector<int> reinserted = tasks;
                std::move(reinserted.begin() + j, reinserted.begin() + i, reinserted.begin() + j + 1);
                reinserted[j] = task;
                output.push_back(Sequence(reinserted));
            }
            for (size_t j = i + 1; j < tasks.size(); j++) { // Insert after i
                std::vector<int> reinserted = tasks;
                std::move(reinserted.begin() + i + 1, reinserted.begin() + j + 1, reinserted.begin() + i);
                reinserted[j] = task;
                output.push_back(Sequence(reinserted));
            }
        }
    }
    else {
        throw std::runtime_error("Neighborhood_size must be 1 (TODO : implement more sizes)");
        throw std::runtime_error("Neighborhood_size must be 1, 2 or 3.");
    }

    return output;
 }

// Accessor implementation
const std::vector<int>& Sequence::get_tasks() const {
    return tasks;
}
