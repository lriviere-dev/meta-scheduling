#ifndef METASOLUTIONS_H
#define METASOLUTIONS_H

#include "Sequence.h"
#include "Policy.h"
#include <vector>
#include <iostream>
#include <unordered_map>
#include <algorithm>

class Policy; //had a circular compile issue that this fixed. Could probably be removed.

// Abstract class for MetaSolutions
class MetaSolution {
public:
    virtual ~MetaSolution() {}
    virtual void print() const = 0;

    //following attributes save scores and sequences for efficiency purposes. Note that ultimately, they depend on a policy, which is ssumed to be unique here.
    std::vector<Sequence> front_sequences; // front of the metasolution : the sequence expressed for each scenario
    std::vector<int> scores; //scores of the expressed sequence in each scenario.
    int score = -1; //the aggregated score
    //is set and marked by policy when evaluated for the first time
    Policy * scored_by = nullptr;
    const DataInstance * scored_for = nullptr;

    virtual void reset_evaluation() { // resets the evaluation to call again (with other policy, or other instance.)
        scored_by = nullptr;
        scored_for = nullptr;
        score = -1;
        scores.clear();
        front_sequences.clear();
    }

    //quantile function to get a specific quantile score among scenarios.
    // WARNING : doesn't check who did the evaluation
    int get_quantile(double quantile, Policy &policy , DataInstance &instance) const {
        if (!scored_by) throw std::runtime_error("metasolution must be scored");
        if (scored_by != &policy) throw std::runtime_error("metasolution was scored for another policy");
        if (scored_for != &instance) throw std::runtime_error("metasolution was scored for another instance");
        if (quantile < 0 || quantile > 1) throw std::invalid_argument("Quantile must be between 0 and 1.");

        //sort the scores
        std::vector<int> sorted_scores = scores;
        std::sort(sorted_scores.begin(), sorted_scores.end());

        size_t index = static_cast<size_t>(std::ceil(quantile * sorted_scores.size())) - 1;
        if (index >= sorted_scores.size()) index = sorted_scores.size() - 1; // clamp

    return sorted_scores[index];
    }

    //basicly a proxy for scores, sorted
    std::vector<int> get_quantiles(Policy &policy, DataInstance &instance) const {
        if (!scored_by) throw std::runtime_error("metasolution must be scored");
        if (scored_by != &policy) throw std::runtime_error("metasolution was scored for another policy");
        if (scored_for != &instance) throw std::runtime_error("metasolution was scored for another instance");
        //sort the scores
        std::vector<int> sorted_scores = scores;
        std::sort(sorted_scores.begin(), sorted_scores.end());

    return sorted_scores;
    }
};


// GroupMetaSolution: A specific meta solution that stores a sequence of permutable groups
class GroupMetaSolution : public MetaSolution {
public:
    // Constructor: Takes a sequence of sets of tasks
    GroupMetaSolution(std::vector<std::vector<int>>& taskGroups)
        : taskGroups(taskGroups) {}

     //operator to check groupSolution equality, could be faster if groups were sorted by default 
    bool operator==(const GroupMetaSolution& other) const {
        if (taskGroups.size() != other.taskGroups.size()) return false;
        for (size_t i = 0; i < taskGroups.size(); ++i) {
            std::vector<int> group1 = taskGroups[i];
            std::vector<int> group2 = other.taskGroups[i];
            std::sort(group1.begin(), group1.end());
            std::sort(group2.begin(), group2.end());
            if (group1 != group2) return false;
        }
        return true;
    }

    void print() const override{
        for (size_t i = 0; i < taskGroups.size(); ++i) {
            const auto& group = taskGroups[i];

            // If it's the first group, print without a prefix
            if (i > 0) {
                std::cout << "|";
            }

            // Print each task in the group, adding '*' between tasks
            for (size_t j = 0; j < group.size(); ++j) {
                std::cout << group[j];
                if (j < group.size() - 1) {
                    std::cout << "*";
                }
            }
        }
    }

    GroupMetaSolution* merge_groups(int group_index) const {

        // Create a copy of the current task groups
        std::vector<std::vector<int>> merged_groups = taskGroups;

        // Merge the selected group with the next group
        merged_groups[group_index].insert(
            merged_groups[group_index].end(),
            std::make_move_iterator(merged_groups[group_index + 1].begin()),
            std::make_move_iterator(merged_groups[group_index + 1].end())
        );

        // Remove the next group as it's now merged
        merged_groups.erase(merged_groups.begin() + group_index + 1);

        // Return a new GroupMetaSolution with the merged groups
        return (new GroupMetaSolution(merged_groups));    
        }

    int nb_groups() const {
        return taskGroups.size();
    }

    int largest_group_size()  {
        size_t max_size=0;
        for (const auto& group : this->get_task_groups()){
            if (group.size()>max_size){
                max_size = group.size();
            }
        }
        return max_size;
    }

    // Method to get the sequence of task groups (sets of tasks) (private)
    const std::vector<std::vector<int>>& get_task_groups() const{
        return taskGroups;
    }

    std::vector<std::vector<int>>& get_task_groups_modifiable(){
        return taskGroups;
    }

    bool operator==(const MetaSolution& other) const {
        // We downcast to the derived class here
        const GroupMetaSolution* groupMeta = dynamic_cast<const GroupMetaSolution*>(&other);
        if (groupMeta) {
            // If it's a GroupMetaSolution, proceed with comparing the group
            return this->compareGroups(*groupMeta);
        }
        return false; // Not equal if it's not the same type
    }

    bool compareGroups(const GroupMetaSolution& other) const {
        if (taskGroups.size() != other.taskGroups.size()) {
            return false; // If the number of groups is different, they are not equal
        }

        for (size_t i = 0; i < taskGroups.size(); ++i) {
            if (taskGroups[i].size() != other.taskGroups[i].size()) {
            return false; // If the size of any group is different, they are not equal
        }
        }

        //ELse we have to do the tedious check work.
        //TODO : could have canonically sorted groups to not have to sort again every time (expensive).
        for (size_t i = 0; i < taskGroups.size(); ++i) { 

            // Copy the groups and sort them to ensure comparison is order-independent within groups
            std::vector<int> sortedThisGroup = taskGroups[i];
            std::vector<int> sortedOtherGroup = other.taskGroups[i];

            std::sort(sortedThisGroup.begin(), sortedThisGroup.end());
            std::sort(sortedOtherGroup.begin(), sortedOtherGroup.end());

            if (sortedThisGroup != sortedOtherGroup) {
                return false; // Groups at this position are not equal
            }
        }

        return true; // All groups match
    }
    
private:
    std::vector<std::vector<int>> taskGroups; // A sequence of sets of tasks
};

//introducing hashes for groupMetaSolutions
namespace std {
    template <>
    struct hash<GroupMetaSolution> {
        size_t operator()(const GroupMetaSolution& gms) const { // Add `const`
            size_t hash_value = 0;
            for (const auto& group : gms.get_task_groups()) {
                std::vector<int> sorted_group = group;
                std::sort(sorted_group.begin(), sorted_group.end());
                for (int task : sorted_group) {
                    hash_value ^= std::hash<int>{}(task) + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
                }
                hash_value ^= 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
            }
            return hash_value;
        }
    };
}

// The simple sequence (JSEQ)
class SequenceMetaSolution:  public MetaSolution {
public:
    SequenceMetaSolution(const Sequence& taskSequence)
        : taskSequence(taskSequence) {}

    SequenceMetaSolution(const std::vector<int>& taskSequence) //alternative definition of a sequence using raw vector (not recommended)
        : taskSequence(Sequence(taskSequence)) {}

    GroupMetaSolution* to_gseq(){
        std::vector<int> raw_seq = this->get_sequence().get_tasks();
        std::vector<std::vector<int>> vectorgroup(raw_seq.size());

        for (size_t i = 0; i < raw_seq.size(); i++) {
            vectorgroup[i] = {raw_seq[i]};  // Create a single task group directly
        }

        return new GroupMetaSolution(vectorgroup);
    }
    SequenceMetaSolution* gen_swap_neighbor(int swap_index, const DataInstance& instance){
        Sequence seq =  this->get_sequence();
        Sequence swaped =  seq.gen_swap_neighbor(swap_index);
        if (swaped.check_precedence_constraints(instance)){
            return new SequenceMetaSolution(swaped);
        }
        else{
            throw InvalidSolutionException();
        }
    }

    std::vector<SequenceMetaSolution> gen_neighbors(int neighborhood_size, const DataInstance& instance){//generates list of nighboring JSEQ solutions (defers to sequence neighborhood)
        std::vector<SequenceMetaSolution> output;
        std::vector<Sequence> neighboring_sequences = taskSequence.neighbours(neighborhood_size);
        for (size_t i = 0; i < neighboring_sequences.size(); i++)
        {
            //if an instance is provided, remove invalid precedence-wise sequences
            if (neighboring_sequences[i].check_precedence_constraints(instance)){
                output.push_back(SequenceMetaSolution(neighboring_sequences[i]));
            }
        }
        return output;
    }
    void print() const override{
        taskSequence.print();
    }
    const Sequence& get_sequence() const {
        return taskSequence;
    }


    bool operator==(const MetaSolution& other) const  {
        // We downcast to the derived class here
        const SequenceMetaSolution* seqMeta = dynamic_cast<const SequenceMetaSolution*>(&other);
        if (seqMeta) {
            // If it's a SeqMeta, proceed with comparing the group
            return this->compareSeqs(*seqMeta);
        }
        return false; // Not equal if it's not the same type
    }


    bool compareSeqs(const SequenceMetaSolution& other) const  {
        return taskSequence.get_tasks() == other.taskSequence.get_tasks();
    }
    
private:
    Sequence taskSequence;
}; 


// You can define more derived classes here...

#endif 