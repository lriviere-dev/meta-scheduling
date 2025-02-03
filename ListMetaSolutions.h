#ifndef LIST_META_H
#define LIST_META_H

#include "Policy.h"
#include "MetaSolutions.h"
#include <vector>
#include <iostream>


//necessary intermediate step to handle different types of ListMetaSolution as one
class ListMetaSolutionBase : public MetaSolution {
public:
    virtual ~ListMetaSolutionBase() {}
    virtual std::vector<MetaSolution*> get_meta_solutions() const = 0;
    virtual void remove_meta_solution_index(size_t index)  = 0;
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

    // Method to add a meta-solution to the list (deprecated, needs to update policy evaluation)
    /*void add_meta_solution(const T& metaSolution) {
        metaSolutions.push_back(metaSolution);
    }*/

    void remove_meta_solution_index(size_t index)  override {
        if (index >= metaSolutions.size()) {
            throw std::out_of_range("Index out of range");
        }
        metaSolutions.erase(metaSolutions.begin() + index);
        reset_evaluation();
    }

    //call when modifying solution in place, removes evaluated tag to re trigger evaluation.
    void reset_evaluation()  {
        scored_by = nullptr;
        score = -1;
        scores.clear();
        front_sequences.clear();
    }

    const std::vector<T>& get_meta_solutions_typed() const {
        return metaSolutions;
    }
    
    // Method to access the list of meta-solutions
    std::vector<MetaSolution*> get_meta_solutions() const override {
        std::vector<MetaSolution*> metaSolutionPtrs;
        for (T& solution : metaSolutions) { 
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

    bool operator==(const MetaSolution& other) const {
        // We downcast to the derived class here
        const ListMetaSolution<T>* listMeta = dynamic_cast<const ListMetaSolution<T>*>(&other);
        if (listMeta) {
            // If it's a listmeta of proper type, proceed
            return this->compareLists(*listMeta);
        }
        return false; // Not equal if it's not the same type
    }

    bool compareLists(const ListMetaSolution<T>& other) const { //very costly check.
        if (metaSolutions.size() != other.metaSolutions.size()) {
            return false; // If sizes differ, they are not equal (they could technically be though)
        }

        // We will check for each metasolution in this list if it exists in the other
        // To ensure strict equality, we must use a comparison operator for metasolutions
        for (const auto& metaSolution : metaSolutions) {
            bool found = false;
            for (const auto& otherMetaSolution : other.metaSolutions) {
                if (metaSolution == otherMetaSolution) {
                    found = true;
                    break; // Found a match, no need to check further for this metasolution
                }
            }
            if (!found) {
                return false; // If we couldn't find a match, they're not equal
            }
        }
        //... and for every metasolution in the other, if it exists in this one
        for (const auto& otherMetaSolution : other.metaSolutions) {
            bool found = false;
            for (const auto& metaSolution : metaSolutions) {
                if (metaSolution == otherMetaSolution) {
                    found = true;
                    break; // Found a match, no need to check further for this metasolution
                }
            }
            if (!found) {
                return false; // If we couldn't find a match, they're not equal
            }
        }

        return true; // All metasolutions match
    }    


private:
    mutable std::vector<T> metaSolutions; // Internal storage for meta-solutions
};


#endif // LIST_META_H