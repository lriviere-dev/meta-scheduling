#ifndef LIST_META_H
#define LIST_META_H

class Policy;

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
    
    //call when modifying solution in place, removes evaluated tag to re trigger evaluation.
    void reset_evaluation() override { // has more things to do than default metasolution re-evaluation
        scored_by = nullptr;
        scored_for = nullptr;
        score = -1;
        scores.clear();
        front_sequences.clear();
        front_indexes.clear(); //also needs to clear this information
    }

    std::vector<int> front_indexes; //index of the metasolution (in the list) used for each scenario

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

   //WARNING : removes the element and also moves around things! COuld be a problem 
   // In BO, we save index and do the removals again in the same order so it yields same output.
    void remove_meta_solution_index(size_t index)  override {
        if (index >= metaSolutions.size()) {
            throw std::out_of_range("Index out of range");
        }
        //metaSolutions.erase(metaSolutions.begin() + index);
        metaSolutions[index] = metaSolutions.back();
        metaSolutions.pop_back();
        reset_evaluation(); //here we could be more cleverer : the sequence/index used in each scenario only changes if it was removed. update_evaluation(int removed_index)
    }


    const int get_meta_solutions_size() const {
        return metaSolutions.size();
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

    // returns the simplified metasolution that contains only the expressed submetasolutions
    // takes an instance as input, outputs the front for that instance.  (same training score if it's the training instance eg)
    ListMetaSolution<T>* front_sub_metasolutions(Policy * policy, const DataInstance &instance)   {
        if (!scored_by){//metasol was not already scored -> score it before continuing
            policy->evaluate_meta(*this, instance);
        }
        else if (scored_by!=policy || scored_for!=&instance){ // else if it is scored but not by this policy, or not for this instance
            reset_evaluation(); //reset it and then score
            policy->evaluate_meta(*this, instance);
        }

        //once it's properly evaluated and scored, extract the front
        std::vector<int> unique_indexes;//
        for (int index : front_indexes) {  // check all the front indexes ( indexes of submetasol used in each scenarios)
            if(std::find(unique_indexes.begin(), unique_indexes.end(), index) == unique_indexes.end()) {
                unique_indexes.push_back(index);//add index if it's not in output list already
            }
        }
        std::sort(unique_indexes.begin(),unique_indexes.end());//for convenience of debugging, we sort. THis has no effect on the training results, but does on the shape on the metasolution
        
        std::vector<T> front_submeta;
        for (size_t i =0; i< unique_indexes.size() ;i++) { 
            front_submeta.push_back(metaSolutions[unique_indexes[i]]);
        }

        return new ListMetaSolution<T>(front_submeta);
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