#ifndef LIST_META_H
#define LIST_META_H

#include "MetaSolutions.h"
#include <vector>
#include <iostream>

//necessary intermediate step to handle different types of ListMetaSolution as one
class ListMetaSolutionBase : public MetaSolution {
public:
    virtual ~ListMetaSolutionBase() {}
    virtual const std::vector<const MetaSolution*> get_meta_solutions() const = 0;
    virtual ListMetaSolutionBase* remove_meta_solution_index(size_t index) const = 0;
};

template <typename T>
class ListMetaSolution : public ListMetaSolutionBase {
public:
    // Constructor: Takes a vector of relevant meta-solutions
    ListMetaSolution(const std::vector<T>& metaSolutions)
        : metaSolutions(metaSolutions) {}


    // Copy constructor
    ListMetaSolution(const ListMetaSolution<T>& other)
        : metaSolutions(other.metaSolutions) {}

    // Method to add a meta-solution to the list
    void add_meta_solution(const T& metaSolution) {
        metaSolutions.push_back(metaSolution);
    }

    ListMetaSolution<T>* remove_meta_solution_index(size_t index) const override {
        if (index >= metaSolutions.size()) {
            throw std::out_of_range("Index out of range");
        }
        // Create a copy of the metaSolutions and remove the specified index
        std::vector<T> newMetaSolutions = metaSolutions;
        newMetaSolutions.erase(newMetaSolutions.begin() + index);
        // Return a new ListMetaSolution with the updated list
        return new ListMetaSolution<T>(newMetaSolutions);
    }

    const std::vector<T>& get_meta_solutions_typed() const {
        return metaSolutions;
    }
    
    // Method to access the list of meta-solutions
    const std::vector<const MetaSolution*> get_meta_solutions() const override {
        std::vector<const MetaSolution*> metaSolutionPtrs;
        for (const auto& solution : metaSolutions) {
            metaSolutionPtrs.push_back(&solution);
        }
        return metaSolutionPtrs;
    }

    // print method
    void print() const override{
        //std::cout << "ListMetaSolution contains " << metaSolutions.size() << " meta-solutions:\n\t";
        std::cout <<"{";
        for (size_t i = 0; i < metaSolutions.size(); ++i) {
            metaSolutions[i].print(); // Assuming T has a print method
            if (i != metaSolutions.size() - 1) { // Don't print a comma after the last element
                std::cout << ", ";
            }
        }
        std::cout <<"}";
    }
private:
    std::vector<T> metaSolutions; // Internal storage for meta-solutions
};


#endif // LIST_META_H