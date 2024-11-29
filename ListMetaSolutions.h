#include "MetaSolutions.h"
#include <vector>
#include <iostream>

//necessary intermediate step to handle different types of ListMetaSolution as one
class ListMetaSolutionBase : public MetaSolution {
public:
    virtual ~ListMetaSolutionBase() {}
    virtual const std::vector<const MetaSolution*> get_meta_solutions() const = 0;
};

template <typename T>
class ListMetaSolution : public ListMetaSolutionBase {
public:
    // Constructor: Takes a vector of relevant meta-solutions
    ListMetaSolution(const std::vector<T>& metaSolutions)
        : metaSolutions(metaSolutions) {}

    // Method to add a meta-solution to the list
    void add_meta_solution(const T& metaSolution) {
        metaSolutions.push_back(metaSolution);
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
        std::cout << "ListMetaSolution contains " << metaSolutions.size() << " meta-solutions:\n\t";
        for (size_t i = 0; i < metaSolutions.size(); ++i) {
            metaSolutions[i].print(); // Assuming T has a print method
            if (i != metaSolutions.size() - 1) { // Don't print a comma after the last element
                std::cout << ", ";
            }
        }
    }
private:
    std::vector<T> metaSolutions; // Internal storage for meta-solutions
};
