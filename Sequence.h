#ifndef SEQUENCE_H
#define SEQUENCE_H

#include <vector>
#include <iostream>
#include "Instance.h"


class InvalidSolutionException : public std::exception {
public:
    const char* what() const noexcept override { // Correct signature
        return "Invalid Solution";
    }
};

class Sequence {
public:
    // Constructor accepting a vector of task IDs
    Sequence(const std::vector<int>& tasks);

    // random constructor
    Sequence(int n, std::mt19937 &rng);

    Sequence();//empty constructor

    // Print function for displaying the sequence
    void print() const;

    std::string to_str() const;

    // Accessor for the task sequence
    const std::vector<int>& get_tasks() const;

    bool check_precedence_constraints(const DataInstance& instance) const;

    Sequence fix_precedence_constraints(const DataInstance& instance) const;

    bool isLexicographicallySmaller(const Sequence& other, const DataInstance& instance, int scenario_id) const;

    std::vector<Sequence> neighbours(int neighborhood_size);

    Sequence gen_swap_neighbor(int swap_index);

private:
    std::vector<int> tasks; // Stores the sequence of tasks
};

#endif // SEQUENCE_H
