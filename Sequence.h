#ifndef SEQUENCE_H
#define SEQUENCE_H

#include <vector>
#include <iostream>
#include "Instance.h"

class Sequence {
public:
    // Constructor accepting a vector of task IDs
    Sequence(const std::vector<int>& tasks);

    // Print function for displaying the sequence
    void print() const;

    // Accessor for the task sequence
    const std::vector<int>& get_tasks() const;

    bool check_precedence_constraints(const DataInstance& instance) const;

    bool isLexicographicallySmaller(const Sequence& other, const DataInstance& scenario) const;

    std::vector<Sequence> neighbours(int neighborhood_size);

private:
    std::vector<int> tasks; // Stores the sequence of tasks
};

#endif // SEQUENCE_H
