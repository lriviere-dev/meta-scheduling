#include "Sequence.h"
#include "Instance.h"

// Constructor implementation
Sequence::Sequence(const std::vector<int>& tasks) : tasks(tasks) {}

Sequence::Sequence(int n, std::mt19937& rng) {
    tasks.resize(n);
    for (int i = 0; i < n; ++i) {
        tasks[i] = i; // Fill with {0, 1, ..., n-1}
    }
    std::shuffle(tasks.begin(), tasks.end(), rng); // Shuffle to get a random sequence
}

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

std::string Sequence::to_str() const {
    std::stringstream ss;
    for (size_t i = 0; i < tasks.size(); i++) {
        if (i != 0) {  // Add a dash between tasks.
            ss << "-";
        }
        ss << tasks[i];
    }
    return ss.str();
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

//moves back tasks as far as needed
Sequence Sequence::fix_precedence_constraints(const DataInstance& instance) const {
    const std::vector<int>& tasks = get_tasks();
    std::vector<int> fixed_tasks = tasks;
    const std::vector<uint8_t>& precedenceConstraints = instance.precedenceConstraints;

    // Check each precedence constraint
    // Check each pair (i, j) where precedenceConstraints[i][j] == 1
    for (size_t i = 0; i < tasks.size()-1; ++i) {
        for (size_t j = tasks.size()-1; j > i; j--) {//going backwards to find the last first
            if (precedenceConstraints[fixed_tasks[j]*instance.N+fixed_tasks[i]] == 1) {
                // Task j must precede task i, but it doesn't => push it back
                //move taks i right after task j
                fixed_tasks.insert(fixed_tasks.begin() + j + 1, fixed_tasks[i]);
                fixed_tasks.erase(fixed_tasks.begin() + i);
                i--;//the push incremented the counter.
            }
        }
    }
    return Sequence(fixed_tasks); 
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

Sequence Sequence::gen_swap_neighbor(int swap_index){
    std::vector<int> swaped = this->get_tasks(); 
    if (swap_index > swaped.size()-2) {
    throw std::runtime_error("swap index out of bounds");
    }
    std::swap(swaped[swap_index], swaped[swap_index + 1]);
    return Sequence(swaped);
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
            output.push_back(Sequence(gen_swap_neighbor(i)));
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
