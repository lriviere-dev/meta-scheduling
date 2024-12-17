#ifndef METASOLUTIONS_H
#define METASOLUTIONS_H

#include "Sequence.h"
#include <vector>
#include <iostream>

// Abstract class for MetaSolutions
class MetaSolution {
public:
    virtual ~MetaSolution() {}
    virtual void print() const = 0; // Pure virtual to ensure all derived classes implement it
    // Additional methods can be added here as needed
};

// GroupMetaSolution: A specific meta solution that stores a sequence of permutable groups
class GroupMetaSolution : public MetaSolution {
public:
    // Constructor: Takes a sequence of sets of tasks
    GroupMetaSolution(const std::vector<std::vector<int>>& taskGroups)
        : taskGroups(taskGroups) {}

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
            merged_groups[group_index + 1].begin(),
            merged_groups[group_index + 1].end()
        );

        // Remove the next group as it's now merged
        merged_groups.erase(merged_groups.begin() + group_index + 1);

        // Return a new GroupMetaSolution with the merged groups
        return (new GroupMetaSolution(merged_groups));    
        }

    const int nb_groups() const {
        return taskGroups.size();
    }

    const int largest_group_size() const {
        int max_size=0;
        for (const auto& group : this->get_task_groups()){
            if (group.size()>max_size){
                max_size = group.size();
            }
        }
        return max_size;
    }

    // Method to get the sequence of task groups (sets of tasks) (private)
    const std::vector<std::vector<int>>& get_task_groups() const {
        return taskGroups;
    }

private:
    std::vector<std::vector<int>> taskGroups; // A sequence of sets of tasks
};


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

        for (int i = 0; i < raw_seq.size(); i++) {
            vectorgroup[i] = {raw_seq[i]};  // Create a single task group directly
        }

        return new GroupMetaSolution(vectorgroup);
    }
    void print() const override{
        taskSequence.print();
    }
    const Sequence& get_sequence() const {
        return taskSequence;
    }

private:
    Sequence taskSequence;
}; 


// The set of sequences sequence (SSEQ) 
//WARNING : FUNCTIONALLY IDENTICAL AS List of JSEQ !! Eventually to remove !
class SequenceSetMetaSolution : public MetaSolution {
public:

    SequenceSetMetaSolution(const std::vector<Sequence>& taskSequences)
        : taskSequences(taskSequences) {}

    SequenceSetMetaSolution(const std::vector<std::vector<int>>& rawTaskSequences) {
            for (const auto& taskSeq : rawTaskSequences) {
                taskSequences.push_back(Sequence(taskSeq));
            } 
        }

    void print() const override{
        std::cout << "{";
        for (size_t i = 0; i < taskSequences.size(); ++i) {
            taskSequences[i].print(); // Use sequence print
            if (i < taskSequences.size() - 1) {
                std::cout << ", "; // Print a comma after each sequence except the last one
            }
        }
        std::cout << "}";
    }

    const std::vector<Sequence>& get_sequence_set() const {
        return taskSequences;
    }

private:
    std::vector<Sequence> taskSequences;
}; 

// You can define more derived classes here...

#endif 